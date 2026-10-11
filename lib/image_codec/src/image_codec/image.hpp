#pragma once
#include <stddef.h>
#include <stdint.h>

namespace image_codec {
constexpr size_t kMaxEncodedBytes = 2 * 1024 * 1024;
constexpr int kMaxDimension = 1024;
// Decoder allocation ceiling; SD input is accounted for separately by the caller.
constexpr size_t kMaxWorkingBytes = 6 * 1024 * 1024;
struct Allocator {
  void* (*allocate)(size_t);
  void* (*reallocate)(void*, size_t);
  void (*release)(void*);
};
// Set once before decoding. Hooks must remain unchanged while images are alive.
void setAllocator(Allocator allocator);
size_t allocatedBytes();
struct Image {
  uint8_t* rgba = nullptr;
  int width = 0, height = 0;
  ~Image();
  Image() = default;
  Image(const Image&) = delete;
  Image& operator=(const Image&) = delete;
  void clear();
};
// Still JPEG (including progressive), PNG and lossy/lossless WebP. No animation.
bool decode(const uint8_t* data, size_t size, Image& output, const char*& error);
struct Fit { int width, height, x, y; };
Fit fit(int width, int height);
uint16_t rgbaToRgb565(const uint8_t* rgba);
}  // namespace image_codec
