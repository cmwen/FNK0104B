#include "screen.hpp"
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include "esp_log.h"
#include "esp_timer.h"
#include "fnk0104b/idf_display.hpp"
#include "fnk0104b/idf_microphone.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

namespace whistle_screen {
namespace {
QueueHandle_t snapshots;
std::atomic<bool> touch_ready{false}, interactive{false}, toggle{false};
constexpr uint16_t bg = 0x0862, white = 0xffff, muted = 0x94b2, green = 0x4eac, blue = 0x347f;
void draw(const Snapshot& s, unsigned page, unsigned busy_seconds) {
  using namespace fnk0104b;
  idfDisplayFill(0, 0, 320, 240, bg);
  idfDisplayText(10, 9, "WHISTLE", white, 2);
  idfDisplayText(190, 15, "OFFLINE  0.1.0", muted);
  const char* status = s.phase == Phase::Ready ? "TAP RECORD TO SPEAK" :
      s.phase == Phase::Recording ? "RECORDING" : s.phase == Phase::Processing ? "TRANSCRIBING" :
      s.phase == Phase::Result ? "TRANSCRIPT" : s.phase == Phase::Error ? "UNAVAILABLE" : "LOADING";
  idfDisplayText(10, 37, status, green, 2);
  char detail[52];
  if (s.phase == Phase::Processing) std::snprintf(detail, sizeof(detail), "PROCESSING %u S - PLEASE WAIT", busy_seconds);
  else std::snprintf(detail, sizeof(detail), "AUDIO %u.%u S / 5 S   LANG %s", s.milliseconds / 1000,
                     (s.milliseconds % 1000) / 100, s.language[0] ? s.language : "AUTO");
  idfDisplayText(10, 62, detail, muted);
  idfDisplayFill(10, 77, 300, 5, 0x2945);
  idfDisplayFill(10, 77, std::min(300, s.peak * 300 / 8192), 5, green);
  // Fixed-width ASCII display; keep complete original UTF-8 on USB serial.
  // A non-ASCII codepoint becomes one '-' instead of consuming several cells.
  char ascii[512] = {};
  size_t n = 0;
  for (const unsigned char* p = reinterpret_cast<const unsigned char*>(s.text); *p && n < sizeof(ascii)-1; ++p) {
    if (*p < 128) ascii[n++] = *p >= 32 && *p < 127 ? static_cast<char>(*p) : ' ';
    else if ((*p & 0xc0) != 0x80) ascii[n++] = '-';
  }
  constexpr unsigned per_page = 50 * 6;
  for (unsigned row = 0; row < 6; ++row) {
    const unsigned start = page * per_page + row * 50;
    if (start >= n) break;
    char line[51] = {};
    std::memcpy(line, ascii + start, std::min(size_t(50), n - start));
    idfDisplayText(10, 96 + row * 15, line, white);
  }
  const bool enabled = s.phase == Phase::Ready || s.phase == Phase::Recording || s.phase == Phase::Result;
  idfDisplayFill(10, 199, 198, 32, enabled ? blue : 0x2945);
  idfDisplayText(25, 210, s.phase == Phase::Recording ? "STOP AND TRANSCRIBE" : enabled ? "RECORD" : "PLEASE WAIT", white);
  idfDisplayFill(220, 199, 90, 32, blue);
  idfDisplayText(232, 210, page ? "PAGE 1" : "PAGE 2", white);
}
void task(void*) {
  Snapshot state;
  bool pressed = false, dirty = true;
  unsigned page = 0, busy_seconds = 0;
  int64_t busy_start = 0, last_draw = 0;
  while (true) {
    Snapshot update;
    if (xQueueReceive(snapshots, &update, pdMS_TO_TICKS(20)) == pdTRUE) {
      if (update.phase != state.phase || std::strcmp(update.text, state.text)) page = 0;
      if (update.phase == Phase::Processing && state.phase != Phase::Processing) busy_start = esp_timer_get_time();
      state = update; dirty = true;
    }
    const auto seconds = state.phase == Phase::Processing ? unsigned((esp_timer_get_time() - busy_start) / 1000000) : 0;
    if (seconds != busy_seconds) { busy_seconds = seconds; dirty = true; }
    if (touch_ready.load()) {
      fnk0104b::IdfTouchPoint point;
      if (fnk0104b::readIdfTouch(point) == ESP_OK) {
        if (point.pressed && !pressed && point.y >= 199 && point.y < 231) {
          if (point.x >= 10 && point.x < 208 && interactive.load()) toggle.store(true);
          else if (point.x >= 220 && point.x < 310) { page = 1 - page; dirty = true; }
        }
        pressed = point.pressed;
      }
    }
    if (dirty && esp_timer_get_time() - last_draw >= 200000) {
      draw(state, page, busy_seconds);
      const auto err = fnk0104b::flushIdfDisplay();
      if (err != ESP_OK) ESP_LOGE("whistle-ui", "display=%s", esp_err_to_name(err));
      last_draw = esp_timer_get_time(); dirty = false;
    }
  }
}
}
bool begin() {
  if (fnk0104b::beginIdfDisplay() != ESP_OK) return false;
  snapshots = xQueueCreate(1, sizeof(Snapshot));
  return snapshots && xTaskCreatePinnedToCore(task, "whistle-ui", 4096, nullptr, 6, nullptr, 0) == pdPASS;
}
void enableTouch() { touch_ready.store(true); }
void publish(const Snapshot& s) {
  interactive.store(s.phase == Phase::Ready || s.phase == Phase::Recording || s.phase == Phase::Result);
  if (!interactive.load()) toggle.store(false);
  xQueueOverwrite(snapshots, &s);
}
bool takeRecordToggle() { return toggle.exchange(false); }
}
