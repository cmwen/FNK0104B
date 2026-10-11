#pragma once

#include <stddef.h>
#include <stdint.h>

namespace storage {
struct BmpInfo {
  uint32_t offset;
  uint32_t stride;
  uint16_t width;
  uint16_t height;
  bool top_down;
  uint32_t rowOffset(uint16_t y) const {
    return offset + (top_down ? y : height - 1 - y) * stride;
  }
};
inline uint16_t le16(const uint8_t* p) {
  return uint16_t(p[0]) | (uint16_t(p[1]) << 8);
}
inline uint32_t le32(const uint8_t* p) {
  return uint32_t(p[0]) | (uint32_t(p[1]) << 8) |
         (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}
// Bound dimensions before arithmetic; reject compression, palettes and truncation.
inline bool parseBmp(const uint8_t* p, size_t size, uint64_t file_size,
                     BmpInfo& info) {
  if (size < 54 || p[0] != 'B' || p[1] != 'M') return false;
  const uint32_t dib = le32(p + 14), offset = le32(p + 10);
  const int32_t width = static_cast<int32_t>(le32(p + 18));
  const int32_t height = static_cast<int32_t>(le32(p + 22));
  if (dib < 40 || uint64_t(14) + dib > offset || offset > file_size ||
      width < 1 || width > 320 || height == 0 || height < -240 || height > 240 ||
      le16(p + 26) != 1 || le16(p + 28) != 24 || le32(p + 30) != 0) return false;
  const uint16_t rows = height < 0 ? -height : height;
  const uint32_t stride = (width * 3 + 3) & ~uint32_t(3);
  if (uint64_t(offset) + uint64_t(stride) * rows > file_size) return false;
  info = {offset, stride, static_cast<uint16_t>(width), rows, height < 0};
  return true;
}
inline uint16_t bgrToRgb565(const uint8_t* p) {
  return ((p[2] & 0xf8) << 8) | ((p[1] & 0xfc) << 3) | (p[0] >> 3);
}
}  // namespace storage
