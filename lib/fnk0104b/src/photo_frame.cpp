#if defined(FNK0104B_ENABLE_PHOTO_FRAME)
#include <fnk0104b/board.hpp>
#include <image_codec/image.hpp>
#include <storage/bmp.hpp>
#include <esp_heap_caps.h>

namespace fnk0104b {
namespace {
uint8_t bmp_row[960];
uint16_t pixels[320];
void* photoAllocate(size_t size) {
  return heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}
void* photoReallocate(void* pointer, size_t size) {
  return heap_caps_realloc(pointer, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}
struct SwapGuard {
  TFT_eSPI& tft;
  bool previous;
  explicit SwapGuard(TFT_eSPI& driver) : tft(driver), previous(driver.getSwapBytes()) {
    tft.setSwapBytes(true);
  }
  ~SwapGuard() { tft.setSwapBytes(previous); }
};
}

bool DisplaySupport::drawPhoto(fs::File& file, const char*& error) {
  error = "unsupported_or_truncated";
  if (!file || file.isDirectory()) return false;
  uint8_t header[54];
  const size_t read = file.read(header, sizeof(header));
  auto& tft = driver();
  SwapGuard swap(tft);
  if (read >= 2 && header[0] == 'B' && header[1] == 'M') {
    storage::BmpInfo info{};
    if (!storage::parseBmp(header, read, file.size(), info)) return false;
    const int x = (tft.width() - info.width) / 2, y = (tft.height() - info.height) / 2;
    tft.fillScreen(TFT_BLACK);
    for (uint16_t line = 0; line < info.height; ++line) {
      if (!file.seek(info.rowOffset(line)) ||
          file.read(bmp_row, info.width * 3) != size_t(info.width * 3)) {
        error = "read_failed"; return false;
      }
      for (uint16_t col = 0; col < info.width; ++col)
        pixels[col] = storage::bgrToRgb565(bmp_row + col * 3);
      tft.pushImage(x, y + line, info.width, 1, pixels);
      yield();
    }
    error = nullptr;
    return true;
  }
  const size_t size = file.size();
  if (!size || size > image_codec::kMaxEncodedBytes) {
    error = "file_exceeds_2MiB"; return false;
  }
  auto* encoded = static_cast<uint8_t*>(photoAllocate(size));
  if (!encoded) { error = "psram_unavailable_or_full"; return false; }
  if (!file.seek(0) || file.read(encoded, size) != size) {
    heap_caps_free(encoded); error = "read_failed"; return false;
  }
  image_codec::setAllocator({photoAllocate, photoReallocate, heap_caps_free});
  image_codec::Image image;
  const bool decoded = image_codec::decode(encoded, size, image, error);
  heap_caps_free(encoded);
  if (!decoded) return false;
  const auto fit = image_codec::fit(image.width, image.height);
  tft.fillScreen(TFT_BLACK);
  for (int y = 0; y < fit.height; ++y) {
    const int source_y = y * image.height / fit.height;
    for (int x = 0; x < fit.width; ++x) {
      const int source_x = x * image.width / fit.width;
      pixels[x] = image_codec::rgbaToRgb565(image.rgba +
          (source_y * image.width + source_x) * 4);
    }
    tft.pushImage(fit.x, fit.y + y, fit.width, 1, pixels);
    yield();
  }
  return true;
}
}  // namespace fnk0104b
#endif
