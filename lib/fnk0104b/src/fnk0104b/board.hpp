#pragma once

#include <stdint.h>
#include <stddef.h>

#if defined(FNK0104B_ENABLE_DISPLAY)
#include <TFT_eSPI.h>
#endif

namespace fnk0104b {

class BoardSupport {
 public:
  void begin(uint32_t serial_baud = 115200);
  void printStartupInfo(const char* firmware_name, const char* firmware_version) const;
  void printPlaceholder(const char* app_name, const char* firmware_version) const;
};

extern BoardSupport board;

#if defined(FNK0104B_ENABLE_DISPLAY)
class DisplaySupport {
 public:
  void begin(uint8_t rotation = 1);
  void setBacklight(bool on);
  TFT_eSPI& driver();
};

extern DisplaySupport display;
#endif

struct TouchPoint {
  int16_t x;
  int16_t y;
  bool pressed;
};

class TouchSupport {
 public:
  bool begin();
  bool read(TouchPoint& point);

 private:
  bool readRegister(uint8_t address, uint8_t& value);
};

extern TouchSupport touch;

struct MicrophoneConfig {
  int mclk_pin;
  int bclk_pin;
  int word_select_pin;
  int data_out_pin;
  int data_in_pin;
  int i2c_sda_pin;
  int i2c_scl_pin;
  uint8_t codec_i2c_address;
};

using MicrophoneStopCheck = bool (*)(void* context);
using MicrophonePeakCallback = void (*)(int32_t peak, void* context);

class MicrophoneSupport {
 public:
  bool begin(const MicrophoneConfig& config);
  bool capture(int16_t* samples, size_t requested_samples,
               size_t& captured_samples, uint32_t timeout_ms,
               MicrophoneStopCheck should_stop = nullptr,
               void* stop_context = nullptr,
               MicrophonePeakCallback on_peak = nullptr,
               void* peak_context = nullptr);
  bool ready() const;

 private:
  bool ready_ = false;
};

extern MicrophoneSupport microphone;

}  // namespace fnk0104b
