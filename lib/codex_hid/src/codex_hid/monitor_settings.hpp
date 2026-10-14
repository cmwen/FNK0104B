#pragma once
#include <cstddef>
#include <cstdint>

namespace codex_hid {
struct MonitorSettings {
  uint8_t volume = 50;
  uint16_t timeout = 30;
  uint8_t slots = 3;
  bool separateVoice = false;
  bool avatars = false;
  bool ledEnabled = true;
  uint16_t ledSeconds = 60;
  uint8_t ledWorkingPattern = 0, ledCompletePattern = 0, ledAttentionPattern = 0;
  uint8_t ledBrightness = 6;
  uint16_t ledPeriodMs = 2000;
};
inline bool decodeMonitorSettings(const uint8_t* bytes, size_t size, MonitorSettings& settings) {
  if (!bytes || size < 4 || bytes[0] < 1 || bytes[0] > 6 || size != (bytes[0] == 6 ? 16 : bytes[0] == 5 ? 10 : size_t(bytes[0]) + 3)) return false;
  const uint16_t timeout = bytes[2] | (uint16_t(bytes[3]) << 8);
  if (bytes[1] > 100 || timeout < 1 || timeout > 120 ||
      (bytes[0] >= 2 && bytes[4] != 3 && bytes[4] != 6) ||
      (bytes[0] >= 3 && bytes[5] > 1) ||
      (bytes[0] >= 4 && bytes[6] > 1) ||
      (bytes[0] >= 5 && (bytes[7] > 1 ||
        (bytes[8] | (uint16_t(bytes[9]) << 8)) < 5 ||
        (bytes[8] | (uint16_t(bytes[9]) << 8)) > 600)) ||
      (bytes[0] == 6 && (bytes[10] > 3 || bytes[11] > 3 || bytes[12] > 3 ||
        bytes[13] < 1 || bytes[13] > 100 ||
        (bytes[14] | (uint16_t(bytes[15]) << 8)) < 500 ||
        (bytes[14] | (uint16_t(bytes[15]) << 8)) > 10000))) return false;
  settings.volume = bytes[1]; settings.timeout = timeout;
  if (bytes[0] >= 2) settings.slots = bytes[4];
  if (bytes[0] >= 3) settings.separateVoice = bytes[5] == 1;
  if (bytes[0] >= 4) settings.avatars = bytes[6] == 1;
  if (bytes[0] >= 5) {
    settings.ledEnabled = bytes[7] == 1;
    settings.ledSeconds = bytes[8] | (uint16_t(bytes[9]) << 8);
  }
  if (bytes[0] == 6) {
    settings.ledWorkingPattern = bytes[10]; settings.ledCompletePattern = bytes[11];
    settings.ledAttentionPattern = bytes[12]; settings.ledBrightness = bytes[13];
    settings.ledPeriodMs = bytes[14] | (uint16_t(bytes[15]) << 8);
  }
  return true;
}
}
