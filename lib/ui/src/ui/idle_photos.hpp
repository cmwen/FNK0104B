#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
namespace ui::photos {
constexpr int width = 320, height = 196, top = 44;
constexpr uint32_t pixelBytes = width * height * 2, fileBytes = pixelBytes + 16;
constexpr unsigned maxFiles = 8;
struct Settings { uint8_t mode = 0; uint16_t delay = 60, interval = 10; };
inline bool decode(const uint8_t* p, size_t n, Settings& out) {
  if (!p || n != 6 || p[0] != 1 || p[1] > 2) return false;
  const uint16_t delay = p[2] | uint16_t(p[3]) << 8;
  const uint16_t interval = p[4] | uint16_t(p[5]) << 8;
  if (delay < 5 || delay > 7200 || interval < 5 || interval > 3600) return false;
  out = {p[1], delay, interval}; return true;
}
inline bool idle(const Settings& settings, uint32_t elapsed, bool busy, bool awake) {
  return awake && !busy && settings.mode && elapsed >= uint32_t(settings.delay) * 1000;
}
inline uint32_t le32(const uint8_t* p) {
  return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}
inline uint32_t crcUpdate(uint32_t crc, const uint8_t* data, size_t size) {
  for (size_t i = 0; i < size; ++i) {
    crc ^= data[i];
    for (int bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1)));
  }
  return crc;
}
inline bool validName(const char* name) {
  if (!name) return false;
  const size_t size = strlen(name);
  if (size < 5 || size > 24 || strcmp(name + size - 4, ".fpf")) return false;
  for (size_t i = 0; i < size - 4; ++i) {
    const char c = name[i];
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9') || c == '-' || c == '_')) return false;
  }
  return true;
}
inline bool validHeader(const uint8_t* h, size_t size) {
  return h && size == fileBytes && !memcmp(h, "FPF1", 4) &&
      (h[4] | unsigned(h[5]) << 8) == width && (h[6] | unsigned(h[7]) << 8) == height &&
      le32(h + 8) == pixelBytes;
}
}
