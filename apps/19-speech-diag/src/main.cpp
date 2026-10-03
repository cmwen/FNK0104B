#include <algorithm>
#include <cstdio>
#include <cstring>
#include "esp_heap_caps.h"
#include "dl_kernel.hpp"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_wn_models.h"
#include "esp_mn_models.h"
#include "esp_mn_speech_commands.h"
#include "model_path.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "fnk0104b/idf_microphone.hpp"
#include "screen.hpp"
#include "audio_frontend.hpp"

namespace {
constexpr char kTag[] = "speech-diag";
constexpr int kWindowMs = 6000;
constexpr const char* kCommands[] = {
    "turn on the light", "turn off the light", "start listening", "stop listening"};
speech_screen::Snapshot screen_state;
AudioFrontEnd frontend;
// Persistent allocation: both recognizers keep their state for the entire test.
void fail(const char* reason) {
  ESP_LOGE(kTag, "speech_status=failed reason=%s", reason);
  screen_state.phase = speech_screen::Phase::Error;
  std::snprintf(screen_state.error, sizeof(screen_state.error), "%s", reason);
  speech_screen::publish(screen_state);
  while (true) vTaskDelay(pdMS_TO_TICKS(1000));
}
}

extern "C" void app_main() {
  if (!speech_screen::begin()) fail("display_initialization_failed");
  // Give USB serial time to enumerate, without requiring a monitor to boot.
  vTaskDelay(pdMS_TO_TICKS(1500));
  ESP_LOGI(kTag, "firmware=speech-diag version=0.3.0 psram_free=%u",
           static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)));
  if (!dl_kernel_lookup("dl_tie728_w8a16_conv2d_11cn")) fail("required_wakenet_kernel_missing");
  srmodel_list_t* models = esp_srmodel_init("model");
  if (!models) fail("model_partition_unavailable");
  char* wake_name = esp_srmodel_filter(models, "wn10", "hiesp");
  char* command_name = esp_srmodel_filter(models, ESP_MN_PREFIX, ESP_MN_ENGLISH);
  char* vad_name = esp_srmodel_filter(models, ESP_VADN_PREFIX, "medium");
  if (!wake_name || !command_name || !vad_name) fail("required_models_missing");
  ESP_LOGI(kTag, "wakenet=%s multinet=%s", wake_name, command_name);
  const esp_wn_iface_t* wake = esp_wn_handle_from_name(wake_name);
  esp_mn_iface_t* commands = esp_mn_handle_from_name(command_name);
  if (!wake || !commands) fail("model_interface_unavailable");
  model_iface_data_t* wake_data = wake->create(wake_name, DET_MODE_90);
  model_iface_data_t* command_data = commands->create(command_name, kWindowMs);
  if (!wake_data || !command_data) fail("model_allocation_failed");
  if (wake->get_samp_rate(wake_data) != 16000 ||
      commands->get_samp_rate(command_data) != 16000) fail("unexpected_sample_rate");
  if (wake->get_channel_num(wake_data) != 1) fail("unexpected_channel_count");
  ESP_ERROR_CHECK(esp_mn_commands_alloc(commands, command_data));
  for (unsigned i = 0; i < sizeof(kCommands) / sizeof(kCommands[0]); ++i) {
    ESP_ERROR_CHECK(esp_mn_commands_add(i + 1, kCommands[i]));
  }
  if (esp_mn_commands_update() != nullptr) fail("command_grammar_rejected");
  esp_mn_commands_print();
  const int wake_samples = wake->get_samp_chunksize(wake_data);
  const int command_samples = commands->get_samp_chunksize(command_data);
  if (wake_samples <= 0 || command_samples <= 0) fail("invalid_frame_size");
  const esp_err_t mic_error = fnk0104b::beginIdfMicrophone();
  if (mic_error != ESP_OK) fail(esp_err_to_name(mic_error));
  if (!frontend.begin(models, vad_name)) fail("vadnet_initialization_failed");
  // The pinned models consume 512 samples per mono frame. Fail explicitly if
  // a future model update changes this contract; never feed mis-sized audio.
  if (frontend.fetchSamples() != wake_samples || frontend.fetchSamples() != command_samples)
    fail("afe_model_frame_size_mismatch");
  screen_state.vad_ready = true;
  screen_state.phase = speech_screen::Phase::Wake;
  ESP_LOGI(kTag, "speech_status=ready wake_phrase=Hi_ESP window_ms=%d", kWindowMs);
  bool listening = false;
  int64_t deadline = 0;
  int64_t notice_until = 0, last_screen = 0;
  int32_t screen_peak = 0;
  int64_t last_report = 0;
  unsigned wakes = 0, detections = 0, timeouts = 0;
  int32_t peak = 0;
  int64_t max_inference_us = 0;
  bool previous_speech = false;
  unsigned speech_segments = 0;
  while (true) {
    const int count = listening ? command_samples : wake_samples;
    afe_fetch_result_t* audio = frontend.fetch();
    if (frontend.error() != ESP_OK) fail(esp_err_to_name(frontend.error()));
    if (!audio || audio->ret_value == ESP_FAIL) fail("afe_fetch_failed");
    if (!audio->data || audio->data_size != count * static_cast<int>(sizeof(int16_t)))
      fail("afe_audio_frame_invalid");
    int16_t* buffer = audio->data;
    const bool speech = audio->vad_state == VAD_SPEECH;
    screen_state.speech = speech;
    if (speech != previous_speech) {
      if (speech) ++speech_segments;
      ESP_LOGI(kTag, "vad=%s speech_segments=%u", speech ? "speech" : "silence", speech_segments);
      previous_speech = speech;
    }
    // All AFE frames reach recognition; vad_cache is not appended because it
    // repeats prior frames already consumed in this continuous audio path.
    for (int i = 0; i < count; ++i) {
      const int32_t value = buffer[i];
      peak = std::max(peak, value < 0 ? -value : value);
      screen_peak = std::max(screen_peak, value < 0 ? -value : value);
    }
    const int64_t started = esp_timer_get_time();
    if (!listening) {
      if (wake->detect(wake_data, buffer) == WAKENET_DETECTED) {
        ++wakes;
        commands->clean(command_data);
        listening = true;
        deadline = esp_timer_get_time() + kWindowMs * 1000LL;
        screen_state.phase = speech_screen::Phase::Listening;
        screen_state.deadline_us = deadline;
        ESP_LOGI(kTag, "wake_detected=%u speech_status=listening", wakes);
      }
    } else {
      const esp_mn_state_t state = commands->detect(command_data, buffer);
      if (state == ESP_MN_STATE_DETECTED) {
        const esp_mn_results_t* result = commands->get_results(command_data);
        if (result && result->num > 0) {
          ++detections;
          const char* phrase = esp_mn_commands_get_string(result->command_id[0]);
          std::snprintf(screen_state.last_command, sizeof(screen_state.last_command), "%s", phrase ? phrase : "UNKNOWN");
          screen_state.probability = result->prob[0];
          screen_state.phase = speech_screen::Phase::Recognized;
          notice_until = esp_timer_get_time() + 2500000;
          ESP_LOGI(kTag, "command_detected=%u id=%d phrase=\"%s\" probability=%.3f",
                   detections, result->command_id[0],
                   phrase ? phrase : "UNKNOWN", result->prob[0]);
        }
        listening = false;
        wake->clean(wake_data);
        commands->clean(command_data);
        ESP_LOGI(kTag, "speech_status=waiting_for_wake");
      } else if (state == ESP_MN_STATE_TIMEOUT || esp_timer_get_time() >= deadline) {
        ++timeouts;
        screen_state.phase = speech_screen::Phase::Timeout;
        notice_until = esp_timer_get_time() + 2500000;
        listening = false;
        wake->clean(wake_data);
        commands->clean(command_data);
        ESP_LOGI(kTag, "command_timeout=%u speech_status=waiting_for_wake", timeouts);
      }
    }
    max_inference_us = std::max(max_inference_us, esp_timer_get_time() - started);
    if (esp_timer_get_time() - last_report >= 5000000) {
      ESP_LOGI(kTag, "state=%s peak=%ld wakes=%u commands=%u timeouts=%u max_inference_us=%lld frame_us=%d vad=%s speech_segments=%u afe_frames=%u heap=%u psram=%u",
               listening ? "listening" : "wake", static_cast<long>(peak), wakes, detections,
               timeouts, static_cast<long long>(max_inference_us), count * 1000000 / 16000,
               speech ? "speech" : "silence", speech_segments, frontend.fedFrames(),
               static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
               static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)));
      last_report = esp_timer_get_time();
      peak = 0;
      max_inference_us = 0;
    }
    const int64_t now = esp_timer_get_time();
    if (!listening && now >= notice_until) screen_state.phase = speech_screen::Phase::Wake;
    if (now - last_screen >= 100000) {
      screen_state.wakes = wakes;
      screen_state.commands = detections;
      screen_state.timeouts = timeouts;
      screen_state.peak = screen_peak;
      speech_screen::publish(screen_state);
      screen_peak = 0;
      last_screen = now;
    }
    // Yield to idle tasks and watchdog while retaining a continuous audio stream.
    vTaskDelay(1);
  }
}
