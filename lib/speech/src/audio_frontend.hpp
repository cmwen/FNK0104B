#pragma once
#include <atomic>
#include <cstddef>
#include "esp_afe_sr_models.h"
#ifdef FNK_SPEECH_EXTERNAL_MICROPHONE
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#endif

// The feed task owns microphone reads. The recognition task consumes continuous AFE
// output, including silence, so VAD delay/cache never clips command prefixes.
class AudioFrontEnd {
 public:
  using ReadAudio = esp_err_t (*)(int16_t*, size_t);
  bool begin(srmodel_list_t* models, char* vad_model, ReadAudio read_audio = nullptr);
  afe_fetch_result_t* fetch(unsigned timeout_ms = 1000);
  void setEnabled(bool enabled) { enabled_.store(enabled); }
  bool paused() const { return !started_.load() || feed_paused_.load(); }
  void reset() { if (iface_ && data_) iface_->reset_buffer(data_); }
  int fetchSamples() const;
  esp_err_t error() const { return error_.load(); }
  unsigned fedFrames() const { return fed_frames_.load(); }
 private:
  static void feedTask(void* context);
#ifdef FNK_SPEECH_EXTERNAL_MICROPHONE
  // Monitor's global frontend reserves an internal stack before BLE/models
  // fragment the heap. Keep microphone/I2S calls on internal memory.
  StaticTask_t feed_task_control_{};
  StackType_t feed_task_stack_[4096 / sizeof(StackType_t)]{};
#endif
  ReadAudio read_audio_ = nullptr;
  const esp_afe_sr_iface_t* iface_ = nullptr;
  esp_afe_sr_data_t* data_ = nullptr;
  int feed_samples_ = 0;
  int16_t* feed_buffer_ = nullptr;
  std::atomic<esp_err_t> error_{ESP_OK};
  std::atomic<unsigned> fed_frames_{0};
  std::atomic<bool> enabled_{true}, feed_paused_{false};
  std::atomic<bool> started_{false};
};
