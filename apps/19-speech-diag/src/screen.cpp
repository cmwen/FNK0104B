#include <algorithm>
#include <cstdio>
#include "screen.hpp"
#include "fnk0104b/idf_display.hpp"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

namespace speech_screen {
namespace {
QueueHandle_t snapshots = nullptr;
constexpr uint16_t kBackground = 0x0863;
constexpr uint16_t kWhite = 0xffff;
constexpr uint16_t kMuted = 0xad75;
constexpr uint16_t kBlue = 0x3dff;
constexpr uint16_t kGreen = 0x67f0;
constexpr uint16_t kYellow = 0xff68;
constexpr uint16_t kRed = 0xf9e7;
using fnk0104b::idfDisplayFill;
using fnk0104b::idfDisplayText;

void draw(const Snapshot& state) {
  char line[64];
  idfDisplayFill(0, 0, 320, 240, kBackground);
  idfDisplayText(10, 9, "VOICE TEST", kWhite, 2);
  idfDisplayText(218, 12, "OFFLINE 0.3.0", kMuted);
  uint16_t accent = kBlue;
  const char* status = "SAY \"HI ESP\"";
  switch (state.phase) {
    case Phase::Starting: status = "LOADING VOICE MODELS"; break;
    case Phase::Wake: break;
    case Phase::Listening: {
      const int seconds = std::max(0, static_cast<int>((state.deadline_us - esp_timer_get_time() + 999999) / 1000000));
      std::snprintf(line, sizeof(line), "LISTENING: %d S", seconds);
      status = line;
      accent = kYellow;
      break;
    }
    case Phase::Recognized: status = "COMMAND HEARD"; accent = kGreen; break;
    case Phase::Timeout: status = "NO COMMAND HEARD"; accent = kYellow; break;
    case Phase::Error: status = "TEST STOPPED"; accent = kRed; break;
  }
  idfDisplayFill(8, 34, 304, 40, accent);
  idfDisplayFill(10, 36, 300, 36, kBackground);
  idfDisplayText(18, 47, status, accent, 2);
  if (state.phase == Phase::Error) {
    idfDisplayText(10, 86, "CHECK USB SERIAL FOR DETAILS", kWhite);
    // Bound and wrap a diagnostic reason to fit the screen.
    char reason[27] = {};
    for (unsigned offset = 0; offset < sizeof(state.error) && state.error[offset]; offset += 26) {
      std::snprintf(reason, sizeof(reason), "%.26s", state.error + offset);
      idfDisplayText(10, 108 + offset / 26 * 16, reason, kRed);
    }
  } else {
    idfDisplayText(10, 83, "1 SAY HI ESP. 2 WHEN LISTENING, SAY:", kMuted);
    idfDisplayText(18, 99, "TURN ON THE LIGHT", kWhite, 2);
    idfDisplayText(18, 119, "TURN OFF THE LIGHT", kWhite, 2);
    idfDisplayText(18, 139, "START LISTENING", kWhite, 2);
    idfDisplayText(18, 159, "STOP LISTENING", kWhite, 2);
    if (state.last_command[0]) {
      std::snprintf(line, sizeof(line), "LAST: %s", state.last_command);
      idfDisplayText(10, 180, line, kGreen);
      std::snprintf(line, sizeof(line), "VADNET %s  MATCH %.0f%%", state.speech ? "SPEECH" : "SILENCE", state.probability * 100);
      idfDisplayText(10, 192, line, state.speech ? kGreen : kMuted);
    } else {
      idfDisplayText(10, 180, "SPEAK CLEARLY NEAR THE MICROPHONE.", kMuted);
      std::snprintf(line, sizeof(line), "VADNET %s - TEST ONLY",
                    !state.vad_ready ? "LOADING" : state.speech ? "SPEECH" : "SILENCE");
      idfDisplayText(10, 192, line, state.speech ? kGreen : kMuted);
    }
  }
  idfDisplayText(10, 209, "MIC", kMuted);
  idfDisplayFill(38, 209, 172, 7, 0x2945);
  // A diagnostic activity meter, not calibrated dB. Saturates at peak 4096.
  const int level = std::min(172, std::max(0, static_cast<int>(state.peak)) * 172 / 4096);
  idfDisplayFill(38, 209, level, 7, state.peak >= 30000 ? kRed : kGreen);
  std::snprintf(line, sizeof(line), "PEAK %ld", static_cast<long>(state.peak));
  idfDisplayText(218, 209, line, kMuted);
  std::snprintf(line, sizeof(line), "WAKE %u  MATCH %u  TIMEOUT %u", state.wakes, state.commands, state.timeouts);
  idfDisplayText(10, 228, line, kMuted);
}
void task(void*) {
  Snapshot state;
  while (true) {
    // Overwrite queue drops obsolete UI snapshots, never audio frames.
    xQueueReceive(snapshots, &state, 0);
    draw(state);
    const esp_err_t error = fnk0104b::flushIdfDisplay();
    if (error != ESP_OK) {
      ESP_LOGE("speech-screen", "display_flush=%s", esp_err_to_name(error));
      vTaskDelete(nullptr);
    }
    vTaskDelay(pdMS_TO_TICKS(200));
  }
}
}
bool begin() {
  const esp_err_t err = fnk0104b::beginIdfDisplay();
  if (err != ESP_OK) {
    ESP_LOGE("speech-screen", "display_init=%s", esp_err_to_name(err));
    return false;
  }
  snapshots = xQueueCreate(1, sizeof(Snapshot));
  if (!snapshots) return false;
  return xTaskCreatePinnedToCore(task, "speech-screen", 4096, nullptr, 2, nullptr, 1) == pdPASS;
}
void publish(const Snapshot& state) {
  if (snapshots) xQueueOverwrite(snapshots, &state);
}
}
