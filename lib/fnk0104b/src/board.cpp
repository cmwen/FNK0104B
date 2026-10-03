#include <Arduino.h>
#include <SD_MMC.h>
#include <Wire.h>
#include <esp_system.h>

#include "fnk0104b/board.hpp"
#include "fnk0104b/pins.hpp"

namespace fnk0104b {

BoardSupport board;
TouchSupport touch;
SdCardSupport sdcard;
namespace {
#if defined(FNK0104B_ENABLE_DISPLAY)
TFT_eSPI tft;
#endif
constexpr uint8_t kTouchPointCount = 0x02;
constexpr uint8_t kTouch1XHigh = 0x03;
constexpr uint8_t kTouch1XLow = 0x04;
constexpr uint8_t kTouch1YHigh = 0x05;
constexpr uint8_t kTouch1YLow = 0x06;
}  // namespace

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

#if defined(FNK0104B_ENABLE_DISPLAY)
DisplaySupport display;

void DisplaySupport::begin(uint8_t rotation) {
  pinMode(pins::display::backlight, OUTPUT);
  setBacklight(true);
  tft.init();
  // Board photos show cyan/magenta/yellow when red/green/blue are sent, even
  // after INVOFF. This fitted panel needs INVON for normal-looking colors.
  tft.invertDisplay(true);
  tft.setRotation(rotation);
}

void DisplaySupport::setBacklight(bool on) {
  digitalWrite(pins::display::backlight, on ? HIGH : LOW);
}

bool DisplaySupport::readRowRgb(uint16_t y, uint8_t* pixels, size_t capacity) {
  if (!pixels || y >= tft.height() || capacity < static_cast<size_t>(tft.width()) * 3) return false;
  tft.readRectRGB(0, y, tft.width(), 1, pixels);
  return true;
}

TFT_eSPI& DisplaySupport::driver() { return tft; }
#endif

bool TouchSupport::begin() {
  pinMode(pins::touch::interrupt, INPUT);
  pinMode(pins::touch::reset, OUTPUT);
  digitalWrite(pins::touch::reset, LOW);
  delay(10);
  digitalWrite(pins::touch::reset, HIGH);
  delay(500);

  Wire.begin(pins::touch::i2c_sda, pins::touch::i2c_scl);
  Wire.beginTransmission(pins::touch::i2c_address);
  const uint8_t status = Wire.endTransmission();
  Serial.printf("touch_i2c=%s address=0x%02X\n", status == 0 ? "ready" : "not_found",
                pins::touch::i2c_address);
  return status == 0;
}

bool TouchSupport::readRegister(uint8_t address, uint8_t& value) {
  Wire.beginTransmission(pins::touch::i2c_address);
  Wire.write(address);
  if (Wire.endTransmission(false) != 0) {
    return false;
  }

  // The Freenove FT6336U driver allows the controller 10 ms between selecting
  // a register and requesting its value.
  delay(10);
  if (Wire.requestFrom(pins::touch::i2c_address, 1) != 1 || !Wire.available()) {
    return false;
  }
  value = static_cast<uint8_t>(Wire.read());
  return true;
}

bool TouchSupport::read(TouchPoint& point) {
  uint8_t count = 0;
  if (!readRegister(kTouchPointCount, count)) {
    return false;
  }
  count &= 0x0F;
  if (count == 0) {
    point.pressed = false;
    return true;
  }

  uint8_t x_high = 0;
  uint8_t x_low = 0;
  uint8_t y_high = 0;
  uint8_t y_low = 0;
  if (!readRegister(kTouch1XHigh, x_high) || !readRegister(kTouch1XLow, x_low) ||
      !readRegister(kTouch1YHigh, y_high) || !readRegister(kTouch1YLow, y_low)) {
    return false;
  }

  const int16_t raw_x = static_cast<int16_t>(((x_high & 0x0F) << 8) | x_low);
  const int16_t raw_y = static_cast<int16_t>(((y_high & 0x0F) << 8) | y_low);

  // Landscape rotation 1 follows the FNK0104B touch example's transform.
  int16_t x = raw_y;
  int16_t y = pins::display::native_width - raw_x;
  x = constrain(x, 0, pins::display::native_height - 1);
  y = constrain(y, 0, pins::display::native_width - 1);
  point = {x, y, true};
  return true;
}

bool SdCardSupport::begin() {
  if (ready_) return true;

  const bool pins_set = SD_MMC.setPins(
      pins::sd::clock, pins::sd::command, pins::sd::data0, pins::sd::data1,
      pins::sd::data2, pins::sd::data3);
  Serial.printf("sd_pins=%s mode=4-bit\n", pins_set ? "configured" : "failed");
  if (!pins_set) return false;

  // Mount without automatic formatting so an unsupported card cannot be
  // erased as a side effect of initialization.
  ready_ = SD_MMC.begin("/sdcard", false, false, SDMMC_FREQ_DEFAULT, 8);
  Serial.printf("sd_status=%s\n", ready_ ? "mounted" : "mount_failed");
  return ready_;
}

bool SdCardSupport::ready() const { return ready_; }

uint64_t SdCardSupport::cardBytes() const {
  return ready_ ? SD_MMC.cardSize() : 0;
}

uint64_t SdCardSupport::filesystemBytes() const {
  return ready_ ? SD_MMC.totalBytes() : 0;
}

uint64_t SdCardSupport::usedBytes() const {
  return ready_ ? SD_MMC.usedBytes() : 0;
}

fs::File SdCardSupport::open(const char* path) const {
  if (!ready_ || path == nullptr) return fs::File();
  return SD_MMC.open(path, FILE_READ);
}

}  // namespace fnk0104b
