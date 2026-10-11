#if defined(FNK0104B_ENABLE_MONITOR_PHOTOS)
#include <fnk0104b/monitor_photos.hpp>
#include <fnk0104b/board.hpp>
#include <ui/idle_photos.hpp>
#include <SD_MMC.h>
#include <esp_heap_caps.h>
#include <errno.h>
namespace fnk0104b::monitor_photos {
namespace {
constexpr char root[] = "/monitor-photos";
constexpr char pending[] = "/monitor-photos/.upload";
fs::File upload;
bool recovered = false;
String path(const char* name) { return String(root) + "/" + name; }
}
bool mount() {
  if (!sdcard.begin()) return false;
  if (!SD_MMC.exists(root) && !SD_MMC.mkdir(root)) {
    Serial.printf("monitor_photos directory_failed errno=%d heap=%u\n", errno, unsigned(ESP.getFreeHeap()));
    return false;
  }
  if (!recovered) {
    auto dir = SD_MMC.open(root);
    if (!dir || !dir.isDirectory()) return false;
    for (auto file = dir.openNextFile(); file; file = dir.openNextFile()) {
      String name(file.name()); name = name.substring(name.lastIndexOf('/') + 1);
      file.close();
      if (!name.endsWith(".bak")) continue;
      const String original = name.substring(0, name.length() - 4);
      if (!ui::photos::validName(original.c_str())) continue;
      if (SD_MMC.exists(path(original.c_str()))) SD_MMC.remove(path(name.c_str()));
      else SD_MMC.rename(path(name.c_str()), path(original.c_str()));
    }
    recovered = true;
  }
  return true;
}
bool scan(char names[8][25], unsigned& count) {
  count = 0;
  if (!mount()) return false;
  auto dir = SD_MMC.open(root);
  if (!dir || !dir.isDirectory()) return false;
  while (count < 8) {
    auto file = dir.openNextFile();
    if (!file) break;
    const String basename = String(file.name()).substring(String(file.name()).lastIndexOf('/') + 1);
    if (!file.isDirectory() && ui::photos::validName(basename.c_str()) && file.size() == ui::photos::fileBytes)
      strlcpy(names[count++], basename.c_str(), 25);
  }
  for (unsigned i = 1; i < count; ++i)
    for (unsigned j = i; j && strcmp(names[j], names[j - 1]) < 0; --j) {
      char tmp[25]; memcpy(tmp, names[j], 25); memcpy(names[j], names[j - 1], 25); memcpy(names[j - 1], tmp, 25);
    }
  return true;
}
void abort() { if (upload) upload.close(); if (sdcard.ready()) SD_MMC.remove(pending); }
bool beginUpload(const char* name) {
  if (!ui::photos::validName(name) || !mount()) return false;
  unsigned count; char names[8][25];
  if (!scan(names, count) || (count == 8 && !SD_MMC.exists(path(name)))) return false;
  abort(); upload = SD_MMC.open(pending, FILE_WRITE); return bool(upload);
}
bool write(const uint8_t* data, size_t size) { return upload && upload.write(data, size) == size; }
bool commit(const char* name) {
  if (!ui::photos::validName(name) || !upload) return false;
  upload.flush(); upload.close();
  const String target = path(name);
  const String backup = target + ".bak";
  const bool exists = SD_MMC.exists(target);
  // Keep the previous completed file until the new file has been renamed.
  if (exists) {
    SD_MMC.remove(backup);
    if (!SD_MMC.rename(target, backup)) return false;
  }
  if (!SD_MMC.rename(pending, target)) {
    if (exists) SD_MMC.rename(backup, target);
    return false;
  }
  if (exists) SD_MMC.remove(backup);
  return true;
}
bool remove(const char* name) { return ui::photos::validName(name) && mount() && SD_MMC.remove(path(name)); }
fs::File open(const char* name) {
  return ui::photos::validName(name) && mount() ? SD_MMC.open(path(name), FILE_READ) : fs::File();
}
uint16_t* allocateFrame() { return static_cast<uint16_t*>(heap_caps_malloc(ui::photos::pixelBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)); }
void releaseFrame(uint16_t* pixels) { heap_caps_free(pixels); }
void drawFrame(uint16_t* pixels) {
  auto& tft = display.driver();
  const bool swap = tft.getSwapBytes(); tft.setSwapBytes(true);
  for (int y = 0; y < ui::photos::height; y += 4) {
    tft.pushImage(0, ui::photos::top + y, ui::photos::width, 4, pixels + y * ui::photos::width);
    yield();
  }
  tft.setSwapBytes(swap);
}
}
#endif
