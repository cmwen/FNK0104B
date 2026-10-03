#include <cstdio>
#include <cstring>
#include <atomic>
#include "fnk0104b/idf_microphone.hpp"
#include "fnk0104b/idf_display.hpp"
#include "fnk0104b/idf_storage.hpp"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
namespace {
std::atomic<bool> echo{false};
void audioTask(void*) {
  bool playing = false;
  while (true) {
    bool enabled = echo.load();
    if (enabled != playing) {
      ESP_ERROR_CHECK(fnk0104b::setIdfSpeakerEnabled(enabled));
      playing = enabled;
    }
    int16_t pcm[320];
    ESP_ERROR_CHECK(fnk0104b::readIdfMicrophone(pcm, 320));
    if (playing) ESP_ERROR_CHECK(fnk0104b::writeIdfSpeaker(pcm, 320));
    vTaskDelay(1);
  }
}
}
extern "C" void app_main() {
  ESP_ERROR_CHECK(fnk0104b::beginIdfAudio());
  ESP_ERROR_CHECK(fnk0104b::beginIdfTouch());
  ESP_ERROR_CHECK(fnk0104b::beginIdfDisplay());
  const esp_err_t sd = fnk0104b::mountIdfSdCard();
  // Exclusively create a diagnostic file; preserve any existing file.
  bool file_ok = false;
  if (sd == ESP_OK) {
    FILE* file = std::fopen("/sdcard/recorder-io-test.txt", "wx");
    if (file) {
      const char text[] = "FNK0104B recorder SD diagnostic\n";
      file_ok = std::fwrite(text, 1, sizeof(text) - 1, file) == sizeof(text) - 1;
      if (std::fclose(file)) file_ok = false;
      file = std::fopen("/sdcard/recorder-io-test.txt", "rb");
      if (file) {
        char readback[sizeof(text)] = {};
        file_ok &= std::fread(readback, 1, sizeof(text) - 1, file) == sizeof(text) - 1 && !std::strcmp(text, readback);
        std::fclose(file);
      } else file_ok = false;
    }
  }
  ESP_LOGI("recorder-io", "sd=%s file_roundtrip=%s touch=ready audio=ready", esp_err_to_name(sd), file_ok ? "passed" : "not_tested_or_failed");
  ESP_ERROR_CHECK(xTaskCreatePinnedToCore(audioTask, "io-audio", 4096, nullptr, 4, nullptr, 0) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
  bool playing = false;
  int64_t last = 0;
  while (true) {
    fnk0104b::IdfTouchPoint point;
    const esp_err_t touch = fnk0104b::readIdfTouch(point);
    if (touch == ESP_OK && point.pressed != playing) {
      playing = point.pressed;
      echo.store(playing);
    }
    if (esp_timer_get_time() - last > 200000) {
      fnk0104b::idfDisplayFill(0, 0, 320, 240, 0x0862);
      fnk0104b::idfDisplayText(10, 20, "RECORDER I/O TEST", 0xffff, 2);
      fnk0104b::idfDisplayText(10, 60, sd == ESP_OK ? "SD MOUNTED" : "INSERT FAT32 SD AND RESTART", 0xffff);
      fnk0104b::idfDisplayText(10, 85, file_ok ? "SD WRITE/READ PASSED" : "SD FILE TEST NOT VERIFIED", 0xffff);
      fnk0104b::idfDisplayText(10, 120, "HOLD SCREEN FOR MIC PLAYBACK", 0xffff);
      fnk0104b::idfDisplayText(10, 145, "CONNECT SPEAKER. KEEP VOLUME LOW.", 0xffff);
      char text[40]; std::snprintf(text, sizeof(text), "TOUCH X=%d Y=%d %s", point.x, point.y, playing ? "ON" : "OFF");
      fnk0104b::idfDisplayText(10, 180, text, 0xffff);
      ESP_ERROR_CHECK(fnk0104b::flushIdfDisplay()); last = esp_timer_get_time();
    }
    vTaskDelay(1);
  }
}
