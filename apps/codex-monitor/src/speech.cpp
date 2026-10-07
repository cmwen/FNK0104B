#include "monitor_speech.hpp"
#include <Arduino.h>
#include <algorithm>
#include <cstring>
#include "audio_frontend.hpp"
#include "voice_capture_gate.hpp"
#include "monitor_commands.hpp"
#include "fnk0104b/board.hpp"
#include "fnk0104b/audio_input.hpp"
#include "dl_kernel.hpp"
#include "esp_wn_models.h"
#include "esp_mn_models.h"
#include "esp_mn_speech_commands.h"
#include "model_path.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "freertos/queue.h"

namespace monitor_speech {
namespace {
AudioFrontEnd frontend;
SemaphoreHandle_t peripheral, audio_mutex, capture_mutex;
QueueHandle_t events;
std::atomic<bool>* busy;
std::atomic<bool> available{false}, in_window{false}, vad{false};
std::atomic<bool> enabled{true}, suspended{false};
std::atomic<uint32_t> quiet_until{0};
std::atomic<uint8_t> input_level{0};
int16_t* capture_buffer = nullptr;  // Protected by capture_mutex.
size_t capture_capacity = 0, capture_count = 0;
bool capture_finished = false;
StaticTask_t worker_control;
StackType_t worker_stack[16384 / sizeof(StackType_t)];
std::atomic<const char*> failure_reason{"starting"};
RTC_NOINIT_ATTR uint32_t startup_stage;
constexpr uint32_t kStartupStageMagic = 0x53525000;
void stage(uint8_t value) {
  startup_stage = kStartupStageMagic | value;
  Serial.printf("monitor_speech startup_stage=%u heap=%u\n", value, unsigned(ESP.getFreeHeap()));
}
speech::VoiceCaptureGate capture_gate;

void emit(Event event) { xQueueSend(events, &event, 0); }
void fail(const char* reason) {
  frontend.setEnabled(false);
  Serial.printf("monitor_speech state=error reason=%s\n", reason);
  failure_reason = reason;
  available = false;
  input_level = 0;
  in_window = false;
  emit(Event::Error);
}
esp_err_t readAudio(int16_t* pcm, size_t samples) {
  return fnk0104b::audio_input::read(pcm, samples);
}
void worker(void*) {
  while (!enabled.load()) { suspended = true; delay(20); }
  suspended = false;
  Serial.printf("monitor_speech startup=models heap=%u psram=%u\n", unsigned(ESP.getFreeHeap()), unsigned(ESP.getFreePsram()));
  stage(0);
  if (!dl_kernel_lookup("dl_tie728_w8a16_conv2d_11cn")) { fail("wake_kernel_missing"); vTaskDelete(nullptr); return; }
  stage(1);
  auto* models = esp_srmodel_init("model");
  stage(2);
  char* wn = models ? esp_srmodel_filter(models, "wn10", "hiesp") : nullptr;
  char* mn = models ? esp_srmodel_filter(models, ESP_MN_PREFIX, ESP_MN_ENGLISH) : nullptr;
  char* vn = models ? esp_srmodel_filter(models, ESP_VADN_PREFIX, "medium") : nullptr;
  if (!wn || !mn || !vn) { fail("models_missing"); vTaskDelete(nullptr); return; }
  auto* wake = esp_wn_handle_from_name(wn);
  auto* commands = esp_mn_handle_from_name(mn);
  stage(3);
  auto* wake_data = wake ? wake->create(wn, DET_MODE_90) : nullptr;
  stage(4);
  auto* command_data = commands ? commands->create(mn, speech::kCommandWindowMs) : nullptr;
  stage(5);
  if (!wake_data || !command_data) { fail("model_allocation_failed"); vTaskDelete(nullptr); return; }
  if (esp_mn_commands_alloc(commands, command_data) != ESP_OK) { fail("grammar_allocation_failed"); vTaskDelete(nullptr); return; }
  for (unsigned i = 0; i < speech::kMonitorCommandCount; ++i)
    if (esp_mn_commands_add(i + 1, speech::kMonitorCommands[i]) != ESP_OK) { fail("grammar_add_failed"); vTaskDelete(nullptr); return; }
  if (esp_mn_commands_update()) { fail("grammar_rejected"); vTaskDelete(nullptr); return; }
  stage(6);
  if (!frontend.begin(models, vn, readAudio)) { fail("frontend_failed"); vTaskDelete(nullptr); return; }
  stage(7);
  const int samples = frontend.fetchSamples();
  if (samples <= 0 || samples != wake->get_samp_chunksize(wake_data) ||
      samples != commands->get_samp_chunksize(command_data) ||
      wake->get_samp_rate(wake_data) != 16000 || commands->get_samp_rate(command_data) != 16000 ||
      wake->get_channel_num(wake_data) != 1) {
    fail("model_audio_contract_mismatch"); vTaskDelete(nullptr); return;
  }
  failure_reason = "none";
  available = true;
  Serial.printf("monitor_speech state=ready wake=Hi_ESP wakenet=%s vadnet=%s multinet=%s\n", wn, vn, mn);
  uint32_t deadline = 0, last_report = millis();
  int64_t max_inference_us = 0;
  bool previous_vad = false, was_busy = false;
  bool resetOnResume = false;
  for (;;) {
    if (!enabled.load()) {
      frontend.setEnabled(false);
      // Drain buffered output so a feed already blocked by backpressure can
      // finish and acknowledge the pause. No recognition runs on these frames.
      while (!frontend.paused() && frontend.error() == ESP_OK) frontend.fetch(20);
      if (!suspended.load()) {
        fnk0104b::audio_input::unsubscribeSpeech();
        frontend.reset(); wake->clean(wake_data); commands->clean(command_data);
        in_window = false; vad = false; input_level = 0;
        xQueueReset(events);
        suspended = true; // Acknowledge only after every local speech consumer is quiescent.
        Serial.println("monitor_speech state=paused reason=usb_micro");
      }
      resetOnResume = true;
      delay(20);
      continue;
    }
    if (resetOnResume) {
      frontend.reset(); wake->clean(wake_data); commands->clean(command_data);
      frontend.setEnabled(true); suspended = false; resetOnResume = false;
      Serial.println("monitor_speech state=resumed");
    }
    auto* audio = frontend.fetch();
    if (!enabled.load()) continue;
    if (!audio && frontend.error() == ESP_OK) continue;

    if (frontend.error() != ESP_OK || !audio || audio->ret_value == ESP_FAIL || !audio->data ||
        audio->data_size != samples * int(sizeof(int16_t))) {
      fail("audio_fetch_failed"); vTaskDelete(nullptr); return;
    }
    const uint8_t measured = speech::microphoneLevel(audio->data, samples);
    const uint8_t previous = input_level.load();
    input_level = measured >= previous ? measured : (previous * 3 + measured) / 4;
    vad = audio->vad_state == VAD_SPEECH;
    if (vad.load() != previous_vad) {
      previous_vad = vad;
      Serial.printf("monitor_speech vad=%s\n", previous_vad ? "speech" : "silence");
    }
    xSemaphoreTake(capture_mutex, portMAX_DELAY);
    if (capture_buffer && !capture_finished) {
      const size_t count = std::min(size_t(samples), capture_capacity - capture_count);
      memcpy(capture_buffer + capture_count, audio->data, count * sizeof(int16_t));
      capture_count += count;
      capture_finished = capture_gate.feed(vad, count) || capture_count == capture_capacity;
    }
    xSemaphoreGive(capture_mutex);
    const int64_t inference_start = esp_timer_get_time();
    uint32_t quiet_deadline = quiet_until.load();
    const bool quiet = quiet_deadline && int32_t(quiet_deadline - millis()) > 0;
    if (quiet_deadline && !quiet) quiet_until.compare_exchange_strong(quiet_deadline, 0);
    const bool is_busy = busy->load() || quiet;
    if (is_busy) {
      in_window = false;
    } else {
      if (was_busy) { wake->clean(wake_data); commands->clean(command_data); }
      if (!in_window) {
        if (wake->detect(wake_data, audio->data) == WAKENET_DETECTED) {
          commands->clean(command_data);
          in_window = true;
          deadline = millis() + speech::kCommandWindowMs;
          emit(Event::Wake);
          Serial.println("monitor_speech state=listening");
        }
      } else {
        const auto state = commands->detect(command_data, audio->data);
        if (state == ESP_MN_STATE_DETECTED) {
          const auto* result = commands->get_results(command_data);
          if (result && result->num && result->command_id[0] >= 1 && result->command_id[0] <= int(speech::kMonitorCommandCount)) {
            emit(static_cast<Event>(result->command_id[0]));
            Serial.printf("monitor_speech command=%d probability=%.3f\n", result->command_id[0], result->prob[0]);
          }
          in_window = false;
        } else if (state == ESP_MN_STATE_TIMEOUT || int32_t(millis() - deadline) >= 0) {
          in_window = false;
          emit(Event::Timeout);
        }
        if (!in_window) { wake->clean(wake_data); commands->clean(command_data); }
      }
    }
    was_busy = is_busy;
    max_inference_us = std::max(max_inference_us, esp_timer_get_time() - inference_start);
    if (millis() - last_report >= 5000) {
      Serial.printf("monitor_speech state=%s afe_frames=%u max_inference_us=%lld frame_us=%d heap=%u psram=%u\n",
          is_busy ? "voice" : in_window ? "listening" : "wake", frontend.fedFrames(),
          static_cast<long long>(max_inference_us), samples * 1000000 / 16000,
          static_cast<unsigned>(ESP.getFreeHeap()), static_cast<unsigned>(ESP.getFreePsram()));
      last_report = millis();
      max_inference_us = 0;
    }
    vTaskDelay(1);
  }
}
}
bool begin(SemaphoreHandle_t mutex, SemaphoreHandle_t audio, std::atomic<bool>* voice_busy) {
  if ((startup_stage & 0xffffff00) == kStartupStageMagic)
    Serial.printf("monitor_speech previous_startup_stage=%u reset_reason=%u\n", unsigned(startup_stage & 0xff), unsigned(esp_reset_reason()));
  peripheral = mutex; audio_mutex = audio; busy = voice_busy;
  events = xQueueCreate(8, sizeof(Event));
  capture_mutex = xSemaphoreCreateMutex();
  const char* reason = !peripheral || !audio_mutex || !busy ? "speech_dependencies_missing" :
    !events ? "speech_queue_failed" : !capture_mutex ? "speech_capture_mutex_failed" : nullptr;
  if (!reason && !xTaskCreateStaticPinnedToCore(worker, "monitor-speech", sizeof(worker_stack), nullptr, 4, worker_stack, &worker_control, 0))
    reason = "speech_task_failed";
  if (reason) {
    failure_reason = reason;
    Serial.printf("monitor_speech state=error reason=%s\n", reason);
    return false;
  }
  return true;
}
bool poll(Event& event) { return events && xQueueReceive(events, &event, 0) == pdTRUE; }
bool ready() { return available && enabled && !suspended; }
void setEnabled(bool value) {
  enabled = value;
  if (!value && events) xQueueReset(events);
}
bool paused() { return suspended || (!available && strcmp(failure_reason.load(), "starting") && frontend.paused()); }
const char* error() { return failure_reason.load(); }
bool listening() { return in_window; }
bool speech() { return vad; }
uint8_t level() { return input_level; }
void quietFor(uint32_t milliseconds) {
  const uint32_t deadline = millis() + milliseconds;
  quiet_until = deadline ? deadline : 1;
}
bool capture(int16_t* pcm, size_t capacity, size_t& captured, const std::atomic<bool>& stop) {
  captured = 0;
  if (!ready() || !pcm || !capacity) return false;
  xSemaphoreTake(capture_mutex, portMAX_DELAY);
  capture_buffer = pcm; capture_capacity = capacity; capture_count = 0;
  capture_finished = false; capture_gate = speech::VoiceCaptureGate{};
  xSemaphoreGive(capture_mutex);
  const uint32_t started = millis();
  bool done = false, voiced = false;
  while (!done) {
    delay(10);
    xSemaphoreTake(capture_mutex, portMAX_DELAY);
    done = capture_finished || stop.load() || !ready() || millis() - started >= speech::kVoiceMaxSeconds * 1000 + 1000;
    if (done) { captured = capture_count; voiced = capture_gate.voiced(); capture_buffer = nullptr; }
    xSemaphoreGive(capture_mutex);
  }
  return ready() && voiced && captured > 0;
}
}
