#include "audio_frontend.hpp"
#include "esp_heap_caps.h"
#include "esp_log.h"
#ifndef FNK_SPEECH_EXTERNAL_MICROPHONE
#include "fnk0104b/idf_microphone.hpp"
#endif
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

bool AudioFrontEnd::begin(srmodel_list_t* models, char* vad_model, ReadAudio read_audio) {
#ifndef FNK_SPEECH_EXTERNAL_MICROPHONE
  if (!read_audio) read_audio = fnk0104b::readIdfMicrophone;
#endif
  if (!read_audio) return false;
  read_audio_ = read_audio;
  if (!models || !vad_model) return false;
  afe_config_t* config = afe_config_init("M", models, AFE_TYPE_SR, AFE_MODE_HIGH_PERF);
  if (!config) return false;
  config->aec_init = false;  // No playback reference or speaker output.
  config->se_init = false;
  config->ns_init = false;
  config->agc_init = false;
  config->wakenet_init = false;  // Retain the existing standalone WakeNet10.
  config->vad_init = true;
  config->vad_model_name = vad_model;  // Explicitly require neural VAD, no fallback.
  config->vad_mode = VAD_MODE_1;
  config->vad_min_speech_ms = 128;
  config->vad_min_noise_ms = 1000;
  config->vad_delay_ms = 128;
  config->afe_linear_gain = 1.0f;
  config->afe_perferred_core = 0;
  config->afe_perferred_priority = 5;
  config->memory_alloc_mode = AFE_MEMORY_ALLOC_MORE_PSRAM;
  iface_ = esp_afe_handle_from_config(config);
  if (iface_) data_ = iface_->create_from_config(config);
  afe_config_free(config);
  if (!iface_ || !data_) return false;
  feed_samples_ = iface_->get_feed_chunksize(data_);
  if (feed_samples_ <= 0 || iface_->get_feed_channel_num(data_) != 1 ||
      iface_->get_fetch_channel_num(data_) != 1 || iface_->get_samp_rate(data_) != 16000) return false;
  feed_buffer_ = static_cast<int16_t*>(heap_caps_malloc(
      feed_samples_ * sizeof(int16_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
  if (!feed_buffer_) return false;
  ESP_LOGI("speech-afe", "vadnet=%s aec=off ns=off agc=off channels=1 feed_samples=%d fetch_samples=%d",
           vad_model, feed_samples_, fetchSamples());
#ifdef FNK_SPEECH_EXTERNAL_MICROPHONE
  const bool started = xTaskCreateStaticPinnedToCore(feedTask, "speech-feed", sizeof(feed_task_stack_),
      this, 5, feed_task_stack_, &feed_task_control_, 0) != nullptr;
#else
  const bool started = xTaskCreatePinnedToCore(feedTask, "speech-feed", 4096, this, 5, nullptr, 0) == pdPASS;
#endif
  if (!started) ESP_LOGE("speech-afe", "feed task allocation failed: internal_free=%u largest=%u",
      static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
      static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)));
  started_ = started;
  return started;
}
void AudioFrontEnd::feedTask(void* context) {
  auto* self = static_cast<AudioFrontEnd*>(context);
  while (true) {
    if (!self->enabled_.load()) {
      self->feed_paused_ = true;
      vTaskDelay(pdMS_TO_TICKS(20));
      continue;
    }
    self->feed_paused_ = false;
    const esp_err_t err = self->read_audio_(self->feed_buffer_, self->feed_samples_);
    // A mode switch can interrupt a pending read; discard that frame/error.
    if (!self->enabled_.load()) continue;
    if (err != ESP_OK) {
      self->error_.store(err);
      self->feed_paused_ = true;
      vTaskDelete(nullptr);
      return;
    }
    if (self->iface_->feed(self->data_, self->feed_buffer_) < 0) {
      self->error_.store(ESP_FAIL);
      self->feed_paused_ = true;
      vTaskDelete(nullptr);
      return;
    }
    self->fed_frames_.fetch_add(1);
    vTaskDelay(1);
  }
}
afe_fetch_result_t* AudioFrontEnd::fetch(unsigned timeout_ms) {
  return iface_->fetch_with_delay(data_, pdMS_TO_TICKS(timeout_ms));
}
int AudioFrontEnd::fetchSamples() const {
  return iface_->get_fetch_chunksize(data_);
}
