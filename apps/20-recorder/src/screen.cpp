#include "screen.hpp"
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include "esp_timer.h"
#include "fnk0104b/idf_display.hpp"
#include "fnk0104b/idf_microphone.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
namespace recorder_screen {
namespace {
std::atomic<bool> touch_ready{false};
QueueHandle_t snapshots = nullptr, events = nullptr;
constexpr uint16_t bg = 0x0862, white = 0xffff, muted = 0x94b2, green = 0x4eac, red = 0xf9a6, blue = 0x347f;
void draw(const Snapshot& s) {
  using namespace fnk0104b;
  idfDisplayFill(0, 0, 320, 240, bg);
  idfDisplayText(10, 8, "VOICE RECORDER", white, 2);
  idfDisplayText(222, 14, "OPUS 24K", muted);
  const char* phase = s.phase == Phase::Ready ? "SAY HI ESP" : s.phase == Phase::Recording ? "RECORDING" :
      s.phase == Phase::Playing ? "PLAYING" : s.phase == Phase::Saving ? "SAVING" : s.phase == Phase::Error ? "CHECK SD / AUDIO" : "LOADING";
  idfDisplayText(10, 35, phase, s.phase == Phase::Recording ? red : green, 2);
  char text[48];
  std::snprintf(text, sizeof(text), "%02u:%02u   VADNET %s", s.seconds / 60, s.seconds % 60, s.speech ? "SPEECH" : "SILENCE");
  idfDisplayText(10, 59, text, muted);
  idfDisplayFill(10, 74, 300, 5, 0x2945);
  idfDisplayFill(10, 74, std::min(300, s.peak * 300 / 4096), 5, green);
  std::snprintf(text, sizeof(text), "%.43s", s.message);
  idfDisplayText(10, 87, text, muted);
  for (unsigned i = 0; i < 3; ++i) {
    const int y = 104 + i * 27;
    if (s.files[i][0]) {
      idfDisplayFill(8, y, 304, 24, s.selected_row == i ? 0x1949 : 0x1083);
      idfDisplayText(16, y + 8, s.files[i], s.selected_row == i ? green : white);
    }
  }
  if (!s.count) idfDisplayText(16, 118, "NO RECORDINGS YET", muted);
  std::snprintf(text, sizeof(text), "FILES %u  PAGE %u/%u", s.count, s.page + 1, std::max(1u, (s.count + 2) / 3));
  idfDisplayText(10, 187, text, muted);
  const char* labels[] = {s.phase == Phase::Recording ? "STOP" : "REC", s.phase == Phase::Playing ? "STOP" : "PLAY", "PREV", "NEXT"};
  for (int i = 0; i < 4; ++i) {
    const int x = 8 + i * 78;
    idfDisplayFill(x, 204, 70, 30, i == 0 ? red : blue);
    idfDisplayText(x + 11, 215, labels[i], white);
  }
}
void touchTask(void*) {
  bool pressed = false;
  int64_t last_report = 0, max_read_us = 0;
  while (true) {
    if (!touch_ready.load()) { vTaskDelay(pdMS_TO_TICKS(10)); continue; }
    const int64_t sampled = esp_timer_get_time();
    fnk0104b::IdfTouchPoint point;
    const esp_err_t err = fnk0104b::readIdfTouch(point);
    max_read_us = std::max(max_read_us, esp_timer_get_time() - sampled);
    if (err != ESP_OK) {
      ESP_LOGE("recorder-ui", "touch=%s", esp_err_to_name(err));
      // Keep the previous pressed state on a bus error, so a held finger
      // does not become a repeated press when the next read succeeds.
    } else {
      if (point.pressed && !pressed) {
        Event event; bool hit = false;
        if (point.y >= 204 && point.y < 234) {
          const int index = (point.x - 8) / 78;
          if (point.x >= 8 && index < 4 && (point.x - 8) % 78 < 70) {
            event.action = static_cast<Action>(index); hit = true;
          }
        } else if (point.y >= 104 && point.y < 185) {
          event = {Action::Select, static_cast<unsigned>((point.y - 104) / 27)}; hit = true;
        }
        if (hit) {
          event.sampled_us = sampled;
          if (xQueueSend(events, &event, 0) != pdTRUE) ESP_LOGW("recorder-ui", "touch_event_queue_full");
        }
      }
      pressed = point.pressed;
    }
    if (sampled - last_report >= 5000000) {
      ESP_LOGI("recorder-ui", "max_touch_read_us=%lld", (long long)max_read_us);
      last_report = sampled; max_read_us = 0;
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
void task(void*) {
  Snapshot s, previous;
  bool drawn = false;
  while (true) {
    // State changes wake rendering immediately; input has its own task.
    if (xQueueReceive(snapshots, &s, pdMS_TO_TICKS(100)) != pdTRUE && drawn) continue;
    const bool phase_changed = !drawn || s.phase != previous.phase;
    const bool list_changed = !drawn || s.count != previous.count || s.page != previous.page ||
        s.selected_row != previous.selected_row || std::memcmp(s.files, previous.files, sizeof(s.files));
    const bool status_changed = phase_changed || s.seconds != previous.seconds || s.speech != previous.speech ||
        s.peak != previous.peak || std::strcmp(s.message, previous.message);
    if (!drawn || status_changed || list_changed) {
      draw(s);
      esp_err_t display = ESP_OK;
      if (!drawn) display = fnk0104b::flushIdfDisplay();
      else {
        if (status_changed) display = fnk0104b::flushIdfDisplayRows(28, 72);
        if (display == ESP_OK && list_changed) display = fnk0104b::flushIdfDisplayRows(100, 100);
        if (display == ESP_OK && phase_changed) display = fnk0104b::flushIdfDisplayRows(200, 40);
      }
      if (display != ESP_OK) ESP_LOGE("recorder-ui", "display=%s", esp_err_to_name(display));
      previous = s; drawn = true;
    }
  }
}
}
bool begin() {
  if (fnk0104b::beginIdfDisplay() != ESP_OK) return false;
  snapshots = xQueueCreate(1, sizeof(Snapshot)); events = xQueueCreate(8, sizeof(Event));
  if (!snapshots || !events) return false;
  if (xTaskCreatePinnedToCore(task, "recorder-ui", 4096, nullptr, 4, nullptr, 1) != pdPASS) return false;
  return xTaskCreatePinnedToCore(touchTask, "recorder-touch", 4096, nullptr, 5, nullptr, 1) == pdPASS;
}
void enableTouch() { touch_ready.store(true); }
void publish(const Snapshot& s) { if (snapshots) xQueueOverwrite(snapshots, &s); }
bool poll(Event& event) { return events && xQueueReceive(events, &event, 0) == pdTRUE; }
}
