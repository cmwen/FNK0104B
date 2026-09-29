#include <Arduino.h>
#include <FS.h>

#include <fnk0104b/board.hpp>

namespace {

constexpr uint16_t rgb565(uint32_t rgb888) {
  return static_cast<uint16_t>(((rgb888 >> 8) & 0xF800) |
                               ((rgb888 >> 5) & 0x07E0) |
                               ((rgb888 >> 3) & 0x001F));
}

// Match the established connectivity-app palette, converted from RGB888 to
// the RGB565 values expected by direct TFT_eSPI drawing calls.
constexpr uint16_t kBackground = rgb565(0x101827);
constexpr uint16_t kPanel = rgb565(0x1C2838);
constexpr uint16_t kPanelRaised = rgb565(0x243449);
constexpr uint16_t kAccent = rgb565(0x58C7D9);
constexpr uint16_t kGreen = rgb565(0x168C69);
constexpr uint16_t kText = rgb565(0xF1F5F9);
constexpr uint16_t kMuted = rgb565(0xA9B7C7);
constexpr uint16_t kWarning = rgb565(0xF59E0B);
constexpr uint8_t kVisibleRows = 4;
constexpr uint8_t kMaxItems = 96;
constexpr uint16_t kTextPageBytes = 352;
constexpr uint8_t kMaxTextLines = 8;
constexpr uint8_t kRawPageBytes = 64;
constexpr uint8_t kTextLineChars = 44;
constexpr int16_t kListTop = 91;
constexpr int16_t kRowHeight = 27;
constexpr int16_t kFooterTop = 207;

template <typename T>
T minValue(T left, T right) {
  return left < right ? left : right;
}

struct Entry {
  String name;
  bool directory = false;
  uint64_t bytes = 0;
};

enum class Screen : uint8_t { kBrowser, kReader, kInfo, kMountError };

Entry entries[kMaxItems];
uint8_t entry_count = 0;
uint8_t first_visible = 0;
String current_path = "/";
String selected_path;
String notice;
Screen screen = Screen::kBrowser;
uint64_t selected_size = 0;
uint32_t reader_offset = 0;
bool selected_is_text = false;
bool previous_touch_pressed = false;
bool touch_ready = false;
uint32_t next_capacity_refresh = 0;
uint32_t next_status_report = 0;

TFT_eSPI& tft() { return fnk0104b::display.driver(); }

String humanBytes(uint64_t bytes) {
  static const char* units[] = {"B", "KiB", "MiB", "GiB", "TiB"};
  double value = static_cast<double>(bytes);
  uint8_t unit = 0;
  while (value >= 1024.0 && unit < 4) {
    value /= 1024.0;
    ++unit;
  }
  char text[24];
  if (unit == 0) {
    snprintf(text, sizeof(text), "%llu B",
             static_cast<unsigned long long>(bytes));
  } else {
    snprintf(text, sizeof(text), value >= 100.0 ? "%.0f %s" : "%.1f %s",
             value, units[unit]);
  }
  return String(text);
}

bool isTextFile(const String& path) {
  const int dot = path.lastIndexOf('.');
  if (dot < 0) return false;
  String extension = path.substring(dot + 1);
  extension.toLowerCase();
  return extension == "txt" || extension == "md" || extension == "csv" ||
         extension == "json" || extension == "log" || extension == "ini" ||
         extension == "xml" || extension == "html" || extension == "htm" ||
         extension == "css" || extension == "cpp" || extension == "h" ||
         extension == "yaml" || extension == "yml" || extension == "toml" ||
         extension == "conf";
}

uint16_t viewerPageBytes() {
  return selected_is_text ? kTextPageBytes : kRawPageBytes;
}

void sortEntries() {
  for (uint8_t i = 1; i < entry_count; ++i) {
    Entry current = entries[i];
    int8_t j = static_cast<int8_t>(i) - 1;
    while (j >= 0) {
      const bool comes_first = current.directory != entries[j].directory
                                   ? current.directory
                                   : current.name.compareTo(entries[j].name) < 0;
      if (!comes_first) break;
      entries[j + 1] = entries[j];
      --j;
    }
    entries[j + 1] = current;
  }
}

void loadDirectory() {
  entry_count = 0;
  first_visible = 0;
  notice = "";

  fs::File directory = fnk0104b::sdcard.open(current_path.c_str());
  if (!directory || !directory.isDirectory()) {
    notice = "Can't open this folder";
    Serial.printf("file_manager_open_directory=failed path=%s\n",
                  current_path.c_str());
    if (directory) directory.close();
    return;
  }

  while (entry_count < kMaxItems) {
    fs::File item = directory.openNextFile();
    if (!item) break;
    entries[entry_count].name = item.name();
    entries[entry_count].directory = item.isDirectory();
    entries[entry_count].bytes = item.size();
    ++entry_count;
    item.close();
  }
  directory.close();
  sortEntries();
  if (entry_count == kMaxItems) notice = "Showing first 96 items";
  Serial.printf("file_manager_directory=%s items=%u\n", current_path.c_str(),
                entry_count);
}

void drawButton(int16_t x, const char* label, uint16_t color,
                int16_t width = 74) {
  tft().fillRoundRect(x, kFooterTop, width, 27, 5, color);
  tft().setTextDatum(MC_DATUM);
  tft().setTextColor(TFT_WHITE, color);
  tft().drawString(label, x + width / 2, kFooterTop + 14, 2);
}

void drawCapacity() {
  const uint64_t total = fnk0104b::sdcard.filesystemBytes();
  const uint64_t used = fnk0104b::sdcard.usedBytes();
  const uint64_t free = total > used ? total - used : 0;
  tft().fillRect(0, 35, 320, 35, kBackground);
  tft().setTextDatum(TL_DATUM);
  tft().setTextColor(kMuted, kBackground);
  const String label = humanBytes(free) + " free / " + humanBytes(total);
  tft().drawString(label.c_str(), 8, 37, 2);

  tft().fillRoundRect(8, 58, 304, 7, 3, kPanelRaised);
  if (total > 0) {
    const uint32_t used_width = static_cast<uint32_t>(
        (static_cast<long double>(used) * 304.0L) / total);
    if (used_width > 0) {
      tft().fillRoundRect(8, 58, minValue<uint32_t>(used_width, 304), 7, 3,
                          used * 100 / total > 90 ? kWarning : kAccent);
    }
  }
}

void drawHeader() {
  tft().fillRect(0, 0, 320, 34, kPanel);
  tft().setTextDatum(TL_DATUM);
  tft().setTextColor(kText, kPanel);
  tft().drawString(screen == Screen::kBrowser ? "SD FILES" : "FILE READER", 9,
                   8, 2);
  if (!fnk0104b::sdcard.ready()) {
    tft().setTextColor(kWarning, kPanel);
    tft().drawRightString("NO CARD", 312, 8, 2);
  } else {
    tft().setTextColor(kGreen, kPanel);
    tft().drawRightString("SD READY", 312, 8, 2);
  }
}

void drawPath() {
  tft().fillRect(0, 70, 320, 19, kBackground);
  tft().setTextDatum(TL_DATUM);
  tft().setTextColor(kAccent, kBackground);
  String display_path = screen == Screen::kBrowser ? current_path : selected_path;
  if (display_path.length() > 39) {
    display_path = "..." + display_path.substring(display_path.length() - 36);
  }
  tft().drawString(display_path.c_str(), 8, 71, 1);
}

void drawFooter() {
  tft().fillRect(0, kFooterTop - 3, 320, 36, kBackground);
  if (screen == Screen::kBrowser) {
    drawButton(4, "HOME", kPanelRaised, 68);
    drawButton(78, "PARENT", kPanelRaised, 72);
    drawButton(156, "PREV", kPanelRaised, 72);
    drawButton(234, "NEXT", kAccent, 82);
  } else if (screen == Screen::kReader || screen == Screen::kInfo) {
    drawButton(4, "BACK", kPanelRaised, 78);
    drawButton(88, "PREV", kPanelRaised, 78);
    drawButton(172, "NEXT", kAccent, 78);
    drawButton(256, "LIST", kPanelRaised, 60);
  } else {
    drawButton(4, "RETRY", kAccent, 78);
    drawButton(88, "HOME", kPanelRaised, 78);
  }
}

void drawBrowser() {
  drawHeader();
  drawCapacity();
  drawPath();
  tft().fillRect(0, 90, 320, 113, kBackground);

  if (notice.length()) {
    tft().setTextColor(kWarning, kBackground);
    tft().drawString(notice.c_str(), 8, 91, 2);
  }
  if (entry_count == 0 && notice.length() == 0) {
    tft().setTextColor(kMuted, kBackground);
    tft().drawString("This folder is empty", 8, 96, 2);
  }

  for (uint8_t row = 0; row < kVisibleRows; ++row) {
    const uint8_t index = first_visible + row;
    if (index >= entry_count) break;
    const int16_t y = kListTop + row * kRowHeight;
    const Entry& item = entries[index];
    tft().fillRoundRect(5, y, 310, 24, 4, kPanel);
    tft().setTextDatum(TL_DATUM);
    tft().setTextColor(item.directory ? kAccent : kText, kPanel);
    const String name = (item.directory ? "DIR  " : "FILE ") + item.name;
    String shown_name = name;
    if (shown_name.length() > 34) {
      shown_name = shown_name.substring(0, 31) + "...";
    }
    tft().drawString(shown_name.c_str(), 10, y + 5, 1);
    if (!item.directory) {
      tft().setTextColor(kMuted, kPanel);
      tft().drawRightString(humanBytes(item.bytes).c_str(), 310, y + 5, 1);
    }
  }

  if (entry_count > kVisibleRows) {
    char page[24];
    snprintf(page, sizeof(page), "%u-%u / %u", first_visible + 1,
             minValue<uint8_t>(first_visible + kVisibleRows, entry_count),
             entry_count);
    tft().setTextDatum(TR_DATUM);
    tft().setTextColor(kMuted, kBackground);
    tft().drawString(page, 315, 196, 1);
  }
  drawFooter();
}

void drawTextContents() {
  fs::File file = fnk0104b::sdcard.open(selected_path.c_str());
  if (!file || file.isDirectory()) {
    tft().setTextColor(kWarning, kBackground);
    tft().drawString("Could not read file", 8, 100, 2);
    if (file) file.close();
    return;
  }
  file.seek(reader_offset);
  char buffer[kTextPageBytes + 1];
  const size_t bytes_read = file.read(reinterpret_cast<uint8_t*>(buffer),
                                      kTextPageBytes);
  file.close();
  buffer[bytes_read] = '\0';

  char line[kTextLineChars + 1];
  uint8_t line_length = 0;
  uint8_t line_count = 0;
  int16_t y = 92;
  for (size_t i = 0; i <= bytes_read && line_count < kMaxTextLines; ++i) {
    const char ch = i == bytes_read ? '\n' : buffer[i];
    if (ch == '\r') continue;
    if (ch == '\n' || line_length == kTextLineChars) {
      line[line_length] = '\0';
      tft().setTextDatum(TL_DATUM);
      tft().setTextColor(kText, kBackground);
      tft().drawString(line, 7, y, 1);
      y += 14;
      line_length = 0;
      ++line_count;
      if (ch == '\n') continue;
    }
    if (i < bytes_read && line_count < kMaxTextLines) {
      line[line_length++] = ch;
    }
  }
  if (bytes_read == 0) {
    tft().setTextColor(kMuted, kBackground);
    tft().drawString("(empty file)", 8, 94, 2);
  }

  char page[40];
  snprintf(page, sizeof(page), "%s  %lu / %llu bytes",
           selected_is_text ? "TEXT" : "RAW PREVIEW",
           static_cast<unsigned long>(reader_offset + bytes_read),
           static_cast<unsigned long long>(selected_size));
  tft().setTextColor(kMuted, kBackground);
  tft().drawString(page, 8, 193, 1);
}

void drawRawPreview() {
  fs::File file = fnk0104b::sdcard.open(selected_path.c_str());
  if (!file) {
    tft().setTextColor(kWarning, kBackground);
    tft().drawString("Could not read file", 8, 100, 2);
    return;
  }
  file.seek(reader_offset);
  uint8_t bytes[kRawPageBytes];
  const size_t count = file.read(bytes, sizeof(bytes));
  file.close();

  tft().setTextColor(kMuted, kBackground);
  tft().drawString("Hex view: 16 bytes per row", 8, 94, 1);
  char row[64];
  for (size_t offset = 0; offset < count; offset += 16) {
    char* cursor = row;
    const size_t end = minValue(offset + 16, count);
    for (size_t i = offset; i < end; ++i) {
      cursor += snprintf(cursor, sizeof(row) - (cursor - row), "%02X ",
                         bytes[i]);
    }
    *cursor = '\0';
    tft().setTextColor(kText, kBackground);
    tft().drawString(row, 8, 111 + (offset / 16) * 14, 1);
  }
  char page[40];
  snprintf(page, sizeof(page), "RAW  %lu / %llu bytes",
           static_cast<unsigned long>(reader_offset + count),
           static_cast<unsigned long long>(selected_size));
  tft().setTextColor(kMuted, kBackground);
  tft().drawString(page, 8, 193, 1);
}

void drawReader() {
  drawHeader();
  drawCapacity();
  drawPath();
  tft().fillRect(0, 90, 320, 114, kBackground);
  if (selected_is_text) {
    drawTextContents();
  } else {
    drawRawPreview();
  }
  drawFooter();
}

void drawMountError() {
  drawHeader();
  tft().fillRect(0, 35, 320, 169, kBackground);
  tft().setTextColor(kWarning, kBackground);
  tft().setTextDatum(TL_DATUM);
  tft().drawString("SD card not mounted", 8, 69, 2);
  tft().setTextColor(kMuted, kBackground);
  tft().drawString("Insert a FAT16/FAT32 card", 8, 100, 2);
  tft().drawString("Then tap RETRY", 8, 125, 2);
  drawFooter();
}

void render() {
  tft().fillScreen(kBackground);
  if (screen == Screen::kBrowser) {
    drawBrowser();
  } else if (screen == Screen::kReader || screen == Screen::kInfo) {
    drawReader();
  } else {
    drawMountError();
  }
}

void openEntry(uint8_t index) {
  if (index >= entry_count) return;
  const Entry& item = entries[index];
  if (item.directory) {
    if (current_path != "/") current_path += "/";
    current_path += item.name;
    loadDirectory();
    render();
    return;
  }

  selected_path = current_path == "/" ? "/" + item.name
                                        : current_path + "/" + item.name;
  selected_size = item.bytes;
  reader_offset = 0;
  selected_is_text = isTextFile(selected_path);
  screen = selected_is_text ? Screen::kReader : Screen::kInfo;
  Serial.printf("file_manager_read=%s bytes=%llu kind=%s\n",
                selected_path.c_str(),
                static_cast<unsigned long long>(selected_size),
                selected_is_text ? "text" : "raw");
  render();
}

void goParent() {
  if (current_path == "/") return;
  const int slash = current_path.lastIndexOf('/');
  current_path = slash <= 0 ? "/" : current_path.substring(0, slash);
  loadDirectory();
  render();
}

void onTouch(int16_t x, int16_t y) {
  if (screen == Screen::kMountError) {
    if (y >= kFooterTop && x < 88) {
      if (fnk0104b::sdcard.begin()) {
        screen = Screen::kBrowser;
        current_path = "/";
        loadDirectory();
      }
      render();
    } else if (y >= kFooterTop && x >= 88 && x < 180) {
      screen = Screen::kBrowser;
      current_path = "/";
      render();
    }
    return;
  }

  if (y >= kFooterTop) {
    if (screen == Screen::kBrowser) {
      if (x < 76) {
        current_path = "/";
        loadDirectory();
      } else if (x < 152) {
        goParent();
        return;
      } else if (x < 230) {
        first_visible = first_visible >= kVisibleRows
                            ? first_visible - kVisibleRows
                            : 0;
      } else if (first_visible + kVisibleRows < entry_count) {
        first_visible = minValue<uint8_t>(first_visible + kVisibleRows,
                                           entry_count - 1);
      }
      render();
    } else {
      if (x < 84 || x >= 250) {
        screen = Screen::kBrowser;
      } else if (x < 168) {
        reader_offset = reader_offset >= viewerPageBytes()
                            ? reader_offset - viewerPageBytes()
                            : 0;
      } else if (reader_offset + viewerPageBytes() < selected_size) {
        reader_offset += viewerPageBytes();
      }
      render();
    }
    return;
  }

  if (screen != Screen::kBrowser || y < kListTop) return;
  const uint8_t row = static_cast<uint8_t>((y - kListTop) / kRowHeight);
  if (row < kVisibleRows) openEntry(first_visible + row);
}

}  // namespace

void setup() {
  fnk0104b::board.begin();
  fnk0104b::board.printStartupInfo("sd-file-manager", "0.1.0");
  fnk0104b::display.begin(1);
  touch_ready = fnk0104b::touch.begin();

  if (fnk0104b::sdcard.begin()) {
    screen = Screen::kBrowser;
    loadDirectory();
    Serial.printf("sd_card_bytes=%llu filesystem_bytes=%llu used_bytes=%llu\n",
                  static_cast<unsigned long long>(fnk0104b::sdcard.cardBytes()),
                  static_cast<unsigned long long>(fnk0104b::sdcard.filesystemBytes()),
                  static_cast<unsigned long long>(fnk0104b::sdcard.usedBytes()));
  } else {
    screen = Screen::kMountError;
  }
  render();
  next_capacity_refresh = millis() + 5000;
  next_status_report = millis() + 5000;
}

void loop() {
  fnk0104b::TouchPoint point{0, 0, false};
  if (fnk0104b::touch.read(point)) {
    if (point.pressed && !previous_touch_pressed) {
      onTouch(point.x, point.y);
    }
    previous_touch_pressed = point.pressed;
  }

  if (static_cast<int32_t>(millis() - next_capacity_refresh) >= 0) {
    if (screen == Screen::kBrowser || screen == Screen::kReader ||
        screen == Screen::kInfo) {
      drawCapacity();
    }
    next_capacity_refresh = millis() + 5000;
  }
  if (static_cast<int32_t>(millis() - next_status_report) >= 0) {
    Serial.printf("file_manager_status=%s touch=%s total_bytes=%llu used_bytes=%llu\n",
                  fnk0104b::sdcard.ready() ? "ready" : "no_card",
                  touch_ready ? "ready" : "not_found",
                  static_cast<unsigned long long>(fnk0104b::sdcard.filesystemBytes()),
                  static_cast<unsigned long long>(fnk0104b::sdcard.usedBytes()));
    next_status_report = millis() + 5000;
  }
  delay(20);
}
