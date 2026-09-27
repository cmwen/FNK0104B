#pragma once

#include <stdint.h>

namespace fnk0104b {

class BoardSupport {
 public:
  void begin(uint32_t serial_baud = 115200);
  void printStartupInfo(const char* firmware_name, const char* firmware_version) const;
  void printPlaceholder(const char* app_name, const char* firmware_version) const;
};

extern BoardSupport board;

}  // namespace fnk0104b
