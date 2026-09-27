#include <Arduino.h>
#include <esp_system.h>

#include "fnk0104b/board.hpp"

namespace fnk0104b {

BoardSupport board;

void BoardSupport::begin(uint32_t serial_baud) {
  Serial.begin(serial_baud);

  // Native USB CDC may enumerate after the MCU starts. Keep the wait bounded
  // so a missing host or monitor never prevents the firmware from booting.
  const uint32_t started_at = millis();
  while (!Serial && (millis() - started_at) < 3000) {
    delay(10);
  }
}

void BoardSupport::printStartupInfo(const char* firmware_name,
                                    const char* firmware_version) const {
  Serial.println("--- FNK0104B firmware ---");
  Serial.printf("firmware=%s\n", firmware_name);
  Serial.printf("version=%s\n", firmware_version);
  Serial.print("chip_model=");
  Serial.println(ESP.getChipModel());
  Serial.printf("flash_bytes=%u\n", static_cast<unsigned>(ESP.getFlashChipSize()));

  const size_t psram_bytes = ESP.getPsramSize();
  if (psram_bytes > 0) {
    Serial.printf("psram_bytes=%u\n", static_cast<unsigned>(psram_bytes));
  } else {
    Serial.println("psram_bytes=0 (unavailable or not initialized)");
  }

  Serial.printf("free_heap_bytes=%u\n", static_cast<unsigned>(ESP.getFreeHeap()));
  Serial.println("status=running");
}

void BoardSupport::printPlaceholder(const char* app_name,
                                    const char* firmware_version) const {
  Serial.println("--- FNK0104B app placeholder ---");
  Serial.printf("firmware=%s\n", app_name);
  Serial.printf("version=%s\n", firmware_version);
  Serial.println("status=placeholder; hardware feature not initialized");
}

}  // namespace fnk0104b
