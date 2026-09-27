#pragma once

#include <stdint.h>

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

}  // namespace fnk0104b
