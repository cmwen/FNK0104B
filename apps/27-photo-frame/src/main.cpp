#include <Arduino.h>
#include <fnk0104b/board.hpp>

// PNG's nested Huffman tables and WebP's decoder need more than the default
// Arduino loop stack. Pixel buffers and decoder heap allocations use PSRAM.
SET_LOOP_TASK_STACK_SIZE(32768);

namespace {
constexpr size_t kMaxPhotos = 128;
constexpr uint32_t kIntervalMs = 10000;
String photos[kMaxPhotos];
size_t count = 0, current = 0;
uint32_t shown_at = 0;
bool touch_ready = false, was_pressed = false, paused = false;
bool have_photo = false;

void message(const char* title, const char* detail) {
  auto& tft = fnk0104b::display.driver();
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(title, 160, 95, 4);
  tft.drawString(detail, 160, 130, 2);
  tft.drawString("Serial: r=reload  n=next  p=pause", 160, 165, 2);
}

bool showPhoto(size_t index) {
  File file = fnk0104b::sdcard.open(photos[index].c_str());
  const uint32_t start = millis();
  const char* error = nullptr;
  if (!fnk0104b::display.drawPhoto(file, error)) {
    Serial.printf("photo_error=%s file=%s\n", error, photos[index].c_str());
    return false;
  }
  Serial.printf("photo_shown=%s index=%u/%u render_ms=%lu\n", photos[index].c_str(),
                unsigned(index + 1), unsigned(count), static_cast<unsigned long>(millis() - start));
  return true;
}

void advance(size_t start) {
  have_photo = false;
  for (size_t tried = 0; tried < count; ++tried) {
    const size_t index = (start + tried) % count;
    if (showPhoto(index)) { current = index; have_photo = true; break; }
  }
  if (!have_photo && count) message("No readable photos", "Check formats and size limits");
  shown_at = millis();
}

void reload() {
  have_photo = false;
  for (size_t i = 0; i < count; ++i) photos[i] = String();
  count = 0;
  if (!fnk0104b::sdcard.begin()) {
    message("SD card unavailable", "Insert a FAT32 card, then reset");
    return;
  }
  File directory = fnk0104b::sdcard.open("/photos");
  if (!directory || !directory.isDirectory()) {
    message("Add your photos", "Copy images into /photos");
    return;
  }
  while (count < kMaxPhotos) {
    File file = directory.openNextFile();
    if (!file) break;
    String name = file.name();
    name = name.substring(name.lastIndexOf('/') + 1);
    String lower = name;
    lower.toLowerCase();
    if (!file.isDirectory() && !name.startsWith(".") &&
        (lower.endsWith(".bmp") || lower.endsWith(".jpg") || lower.endsWith(".jpeg") ||
         lower.endsWith(".png") || lower.endsWith(".webp")))
      photos[count++] = "/photos/" + name;
  }
  // Deterministic filename order independent of FAT directory entry order.
  for (size_t i = 1; i < count; ++i)
    for (size_t j = i; j > 0 && photos[j].compareTo(photos[j - 1]) < 0; --j) {
      String tmp = photos[j]; photos[j] = photos[j - 1]; photos[j - 1] = tmp;
    }
  Serial.printf("photo_count=%u limit=%u\n", unsigned(count), unsigned(kMaxPhotos));
  if (count) advance(0);
  else message("No photos found", "Copy images into /photos");
}
}  // namespace

void setup() {
  fnk0104b::board.begin();
  fnk0104b::board.printStartupInfo("photo-frame", "0.2.0");
  fnk0104b::display.begin(1);
  touch_ready = fnk0104b::touch.begin();
  reload();
}

void loop() {
  while (Serial.available()) {
    const char command = Serial.read();
    if (command == 'r') reload();
    if (command == 'n' && count) advance((current + 1) % count);
    if (command == 'p') {
      paused = !paused; shown_at = millis();
      Serial.printf("slideshow_paused=%u\n", paused);
    }
  }
  if (touch_ready) {
    fnk0104b::TouchPoint point{};
    if (fnk0104b::touch.read(point)) {
      if (point.pressed && !was_pressed && count) advance((current + 1) % count);
      was_pressed = point.pressed;
    }
  }
  if (have_photo && !paused && count > 1 && millis() - shown_at >= kIntervalMs)
    advance((current + 1) % count);
  delay(20);
}
