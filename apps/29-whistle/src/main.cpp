// FNK0104B integration of andrisgauracs/whistle-esp32 (Apache-2.0).
// Original engine and Cactus Compute model credits: ../README.md and lib/whistle.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_partition.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "fnk0104b/idf_microphone.hpp"
#include "nd_quant.h"
#include "nd_whistle.h"
#include "screen.hpp"

namespace {
constexpr unsigned sample_rate = 16000, max_samples = 5 * sample_rate, chunk = 320;
nd_whistle model;
whistle_screen::Snapshot screen;
SemaphoreHandle_t go, done;
nd_row_fn row_fn;
void* row_context;
uint32_t row_start, row_end;
constexpr const char* languages[] = {"en", "de", "fr", "es", "it", "nl", "pl"};
[[noreturn]] void fail(const char* message) {
  ESP_LOGE("whistle", "%s", message);
  screen.phase = whistle_screen::Phase::Error;
  std::snprintf(screen.text, sizeof(screen.text), "%s", message);
  whistle_screen::publish(screen);
  vTaskDelete(nullptr);
  for (;;) vTaskDelay(portMAX_DELAY);
}
void yieldPeriodically(int64_t& last) {
  if (esp_timer_get_time() - last >= 20000) {
    vTaskDelay(1); last = esp_timer_get_time();
  }
}
void worker(void*) {
  int64_t last_yield = 0;
  while (true) {
    xSemaphoreTake(go, portMAX_DELAY);
    row_fn(row_context, row_start, row_end);
    xSemaphoreGive(done);
    // Let idle run on both cores; retain task watchdog protection.
    yieldPeriodically(last_yield);
  }
}
void parallel(nd_row_fn fn, void* context, uint32_t rows) {
  static int64_t last_yield = 0;
  const auto half = rows / 2;
  if (half < 2) fn(context, 0, rows);
  else {
    row_fn = fn; row_context = context; row_start = half; row_end = rows;
    xSemaphoreGive(go);
    fn(context, 0, half);
    xSemaphoreTake(done, portMAX_DELAY);
  }
  yieldPeriodically(last_yield);
}
void memory(const char* phase) {
  ESP_LOGI("whistle", "%s internal_free=%u psram_free=%u psram_largest=%u psram_min=%u",
      phase, unsigned(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)),
      unsigned(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)),
      unsigned(heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM)),
      unsigned(heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM)));
}
void transcribe(float* clip, unsigned count, float floor) {
  screen.phase = whistle_screen::Phase::Processing;
  std::snprintf(screen.text, sizeof(screen.text), "Working entirely on this board");
  whistle_screen::publish(screen);
  uint32_t start = 0, end = count;
  if (count < 1600 || !nd_whistle_find_speech(clip, count, floor, 12.0f, 300, &start, &end)) {
    std::snprintf(screen.text, sizeof(screen.text), "No speech detected. Try closer to the microphone.");
    screen.phase = whistle_screen::Phase::Result;
    whistle_screen::publish(screen);
    ESP_LOGI("whistle", "no_speech");
    return;
  }
  const auto frames = nd_whistle_frames(&model, end - start);
  // The unchanged upstream engine assumes several scratch allocations succeed.
  // Reserve conservative headroom above the host-measured five-second peak;
  // this app has no concurrent heap-heavy radio/storage tasks.
  if (heap_caps_get_free_size(MALLOC_CAP_SPIRAM) < 4 * 1024 * 1024 ||
      heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM) < 3 * 1024 * 1024) {
    screen.phase = whistle_screen::Phase::Result;
    std::snprintf(screen.text, sizeof(screen.text), "Insufficient inference memory. Restart the board.");
    whistle_screen::publish(screen);
    memory("inference_refused");
    return;
  }
  auto* encoded = static_cast<float*>(heap_caps_malloc(size_t(frames) * model.d * sizeof(float), MALLOC_CAP_SPIRAM));
  uint32_t tokens[96];
  memory("inference_start");
  const auto began = esp_timer_get_time();
  int nt = -1;
  if (encoded && nd_whistle_encode(&model, clip + start, end - start, encoded) == 0)
    nt = nd_whistle_decode(&model, encoded, frames, -1, tokens, 96);
  heap_caps_free(encoded);
  screen.language[0] = 0;
  if (nt < 0) std::snprintf(screen.text, sizeof(screen.text), "Transcription failed. Try a shorter recording.");
  else {
    nd_whistle_detok(&model, tokens, unsigned(nt), screen.text, sizeof(screen.text));
    if (model.lang >= 0 && model.lang < 7) std::snprintf(screen.language, sizeof(screen.language), "%s", languages[model.lang]);
    if (!screen.text[0]) std::snprintf(screen.text, sizeof(screen.text), "No words recognized. Try again.");
  }
  screen.phase = whistle_screen::Phase::Result;
  screen.peak = 0;
  whistle_screen::publish(screen);
  ESP_LOGI("whistle", "result lang=%s tokens=%d audio_ms=%u elapsed_ms=%lld", screen.language, nt,
      count / 16, (long long)((esp_timer_get_time() - began) / 1000));
  // USB serial contains the original UTF-8; LCD has a compact ASCII font.
  std::printf("TXT %s\n", screen.text);
  memory("inference_end");
}
void run(void*) {
  const esp_partition_t* part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, static_cast<esp_partition_subtype_t>(0x40), "model");
  esp_partition_mmap_handle_t handle;
  const void* blob = nullptr;
  if (!part || esp_partition_mmap(part, 0, part->size, ESP_PARTITION_MMAP_DATA, &blob, &handle) != ESP_OK)
    fail("Cannot map model. Install the complete whistle environment.");
  if (nd_whistle_open(&model, blob, part->size) != 0 || model.dec_layers != 4)
    fail("Missing or invalid depth-4 model. Run prepare_whistle_model.py then upload.");
  // Unbiased transcription: the LED demo's color keyword bonus and retries are
  // deliberately absent. All seven model languages remain enabled.
  const auto cached = nd_whistle_cache_head(&model, [](size_t n) -> void* { return heap_caps_malloc(n, MALLOC_CAP_SPIRAM); });
  ESP_LOGI("whistle", "model depth=%u head_cache=%u", unsigned(model.depth), unsigned(cached));
  auto* clip = static_cast<float*>(heap_caps_malloc(max_samples * sizeof(float), MALLOC_CAP_SPIRAM));
  if (!clip) fail("Not enough PSRAM for recording");
  int16_t samples[chunk];
  unsigned n = 0, idle_windows = 0, drain = 10;
  float floor_energy = 0;
  bool recording = false;
  screen.phase = whistle_screen::Phase::Ready;
  std::snprintf(screen.text, sizeof(screen.text), "Tap Record, speak, then tap Stop. Up to five seconds per clip.");
  whistle_screen::publish(screen);
  memory("ready");
  int64_t last_ui = 0, last_report = 0;
  while (true) {
    if (fnk0104b::readIdfMicrophone(samples, chunk) != ESP_OK) fail("Microphone read failed");
    if (drain) { --drain; continue; }
    if (whistle_screen::takeRecordToggle()) {
      if (recording) recording = false;
      else {
        recording = true; n = 0;
        screen.milliseconds = 0; screen.language[0] = 0;
        screen.phase = whistle_screen::Phase::Recording;
        std::snprintf(screen.text, sizeof(screen.text), "Speak now. Tap Stop when finished.");
        whistle_screen::publish(screen);
        ESP_LOGI("whistle", "recording");
      }
    }
    float energy = 0;
    int peak = 0;
    for (unsigned i = 0; i < chunk; ++i) {
      peak = std::max(peak, std::abs(int(samples[i])));
      const float v = float(samples[i]) / 32768.0f;
      energy += v * v;
      if (recording && n < max_samples) clip[n++] = v;
    }
    if (!recording && n == 0) {
      // Quietest idle window, slowly rising to follow a changing noise floor.
      const float e = energy / chunk;
      floor_energy = idle_windows ? std::min(e, floor_energy * 1.002f + 1e-12f) : e;
      ++idle_windows;
    }
    if (n && (!recording || n >= max_samples)) {
      recording = false;
      screen.milliseconds = n / 16;
      // Drop touchscreen/press transients, retaining up to 4.92 s of speech.
      if (n > 1280) transcribe(clip + 800, n - 1280, idle_windows >= 25 ? std::sqrt(floor_energy) : 0);
      else transcribe(clip, 0, 0);
      n = 0; drain = 10; idle_windows = 0;
    } else if (esp_timer_get_time() - last_ui >= 200000) {
      screen.peak = peak;
      if (recording) screen.milliseconds = n / 16;
      whistle_screen::publish(screen); last_ui = esp_timer_get_time();
    }
    if (esp_timer_get_time() - last_report >= 5000000) {
      memory("heartbeat"); last_report = esp_timer_get_time();
    }
  }
}
}
extern "C" void app_main() {
  if (!whistle_screen::begin()) { ESP_LOGE("whistle", "display initialization failed"); return; }
  whistle_screen::publish(screen);
  if (fnk0104b::beginIdfMicrophone() != ESP_OK) fail("Microphone initialization failed");
  if (fnk0104b::beginIdfTouch() != ESP_OK) fail("Touch initialization failed");
  whistle_screen::enableTouch();
  go = xSemaphoreCreateBinary(); done = xSemaphoreCreateBinary();
  if (!go || !done || xTaskCreatePinnedToCore(worker, "whistle-rows", 16384, nullptr, 4, nullptr, 1) != pdPASS)
    fail("Cannot start inference worker");
  nd_parallel_rows = parallel;
  if (xTaskCreatePinnedToCore(run, "whistle", 48 * 1024, nullptr, 4, nullptr, 0) != pdPASS)
    fail("Cannot start transcription");
}
