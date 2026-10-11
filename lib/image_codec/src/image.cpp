#include <image_codec/image.hpp>
#include <image_codec/allocator.h>
#include <cstring>
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#define STBI_NO_SIMD
#define STBI_MAX_DIMENSIONS 1024
#define STBI_MALLOC frame_image_malloc
#define STBI_REALLOC frame_image_realloc
#define STBI_FREE frame_image_free
#define STB_IMAGE_IMPLEMENTATION
#include "vendor/stb_image.h"
#include "vendor/libwebp/src/webp/decode.h"

namespace image_codec {
Image::~Image() { clear(); }
void Image::clear() {
  frame_image_free(rgba);
  rgba = nullptr; width = height = 0;
}
bool decode(const uint8_t* data, size_t size, Image& output, const char*& error) {
  output.clear();
  error = "unsupported_or_corrupt";
  if (!data || size < 12 || size > kMaxEncodedBytes) {
    error = "file_size_limit_or_truncated"; return false;
  }
  int width = 0, height = 0, channels = 0;
  const bool webp = std::memcmp(data, "RIFF", 4) == 0 &&
                    std::memcmp(data + 8, "WEBP", 4) == 0;
  if (webp) {
    WebPBitstreamFeatures features{};
    if (WebPGetFeatures(data, size, &features) != VP8_STATUS_OK) return false;
    if (features.has_animation) { error = "animated_webp_unsupported"; return false; }
    width = features.width; height = features.height;
  } else {
    const bool jpeg = data[0] == 0xff && data[1] == 0xd8;
    const uint8_t png_magic[] = {137, 80, 78, 71, 13, 10, 26, 10};
    if ((!jpeg && std::memcmp(data, png_magic, 8) != 0) ||
        !stbi_info_from_memory(data, static_cast<int>(size), &width, &height, &channels)) return false;
  }
  if (width < 1 || height < 1 || width > kMaxDimension || height > kMaxDimension) {
    error = "dimensions_exceed_1024"; return false;
  }
  if (webp) {
    const size_t bytes = static_cast<size_t>(width) * height * 4;
    output.rgba = static_cast<uint8_t*>(frame_image_malloc(bytes));
    if (output.rgba && !WebPDecodeRGBAInto(data, size, output.rgba, bytes, width * 4))
      output.clear();
  } else {
    output.rgba = stbi_load_from_memory(data, static_cast<int>(size), &width, &height, &channels, 4);
  }
  if (!output.rgba) { error = "decode_failed_or_memory_limit"; return false; }
  output.width = width; output.height = height;
  error = nullptr;
  return true;
}
Fit fit(int width, int height) {
  if (width < 1 || height < 1) return {0, 0, 0, 0};
  int w = width, h = height;
  if (w > 320 || h > 240) {
    if (width * 240 >= height * 320) { w = 320; h = height * 320 / width; }
    else { h = 240; w = width * 240 / height; }
    if (!w) w = 1;
    if (!h) h = 1;
  }
  return {w, h, (320 - w) / 2, (240 - h) / 2};
}
uint16_t rgbaToRgb565(const uint8_t* p) {
  // Straight alpha composited over the black letterbox background.
  const uint8_t red = (unsigned(p[0]) * p[3] + 127) / 255;
  const uint8_t green = (unsigned(p[1]) * p[3] + 127) / 255;
  const uint8_t blue = (unsigned(p[2]) * p[3] + 127) / 255;
  return ((red & 0xf8) << 8) | ((green & 0xfc) << 3) | (blue >> 3);
}
}  // namespace image_codec
