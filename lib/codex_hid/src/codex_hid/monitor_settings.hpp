#pragma once
#include <cstddef>
#include <cstdint>

namespace codex_hid {
struct MonitorSettings {
  uint8_t volume = 50;
  uint16_t timeout = 30;
  uint8_t slots = 3;
  bool separateVoice = false;
};
inline bool decodeMonitorSettings(const uint8_t* bytes, size_t size, MonitorSettings& settings) {
  if (!bytes || size < 4 || bytes[0] < 1 || bytes[0] > 3 || size != size_t(bytes[0]) + 3) return false;
  const uint16_t timeout = bytes[2] | (uint16_t(bytes[3]) << 8);
  if (bytes[1] > 100 || timeout < 1 || timeout > 120 ||
      (bytes[0] >= 2 && bytes[4] != 3 && bytes[4] != 6) ||
      (bytes[0] == 3 && bytes[5] > 1)) return false;
  settings.volume = bytes[1]; settings.timeout = timeout;
  if (bytes[0] >= 2) settings.slots = bytes[4];
  if (bytes[0] == 3) settings.separateVoice = bytes[5] == 1;
  return true;
}
}
