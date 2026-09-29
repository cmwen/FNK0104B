#include <Arduino.h>
#include <FS.h>

#include "fnk0104b/board.hpp"

namespace {

constexpr uint8_t kMaxDirectoryDepth = 3;
constexpr uint16_t kMaxListedEntries = 256;
uint16_t listed_entries = 0;
bool card_mounted = false;

void listDirectory(File directory, const String& path, uint8_t depth) {
  while (listed_entries < kMaxListedEntries) {
    File entry = directory.openNextFile();
    if (!entry) {
      break;
    }

    ++listed_entries;
    const char* name = entry.name();
    String entry_path = path;
    if (!entry_path.endsWith("/")) {
      entry_path += "/";
    }
    entry_path += name;

    if (entry.isDirectory()) {
      Serial.printf("DIR  %s/\n", entry_path.c_str());
      if (depth < kMaxDirectoryDepth) {
        listDirectory(entry, entry_path, depth + 1);
      }
    } else {
      Serial.printf("FILE %s (%llu bytes)\n", entry_path.c_str(),
                    static_cast<unsigned long long>(entry.size()));
    }
    entry.close();
  }
}

void printDirectory() {
  listed_entries = 0;
  File root = fnk0104b::sdcard.open("/");
  if (!root || !root.isDirectory()) {
    Serial.println("sd_status=mounted_root_unavailable");
    return;
  }

  Serial.println("sd_directory_begin");
  listDirectory(root, "", 0);
  root.close();
  if (listed_entries == kMaxListedEntries) {
    Serial.println("sd_directory_truncated=true");
  }
  Serial.printf("sd_directory_entries=%u\n", listed_entries);
  Serial.println("sd_directory_end");
}

}  // namespace

void setup() {
  fnk0104b::board.begin();
  fnk0104b::board.printStartupInfo("sd", "0.2.0");

  if (!fnk0104b::sdcard.begin()) {
    Serial.println("sd_status=mount_failed");
    Serial.println("hint=check_card_seating_and_format; FAT16/FAT32 recommended; exFAT may not mount");
    return;
  }

  Serial.println("sd_status=mounted");
  card_mounted = true;
  Serial.printf("sd_card_bytes=%llu\n",
                static_cast<unsigned long long>(fnk0104b::sdcard.cardBytes()));
  Serial.printf("sd_filesystem_bytes=%llu\n",
                static_cast<unsigned long long>(fnk0104b::sdcard.filesystemBytes()));
  Serial.printf("sd_used_bytes=%llu\n",
                static_cast<unsigned long long>(fnk0104b::sdcard.usedBytes()));

  printDirectory();
}

void loop() {
  static uint32_t last_report = 0;
  static uint32_t last_listing = 0;
  if (millis() - last_report >= 5000) {
    last_report = millis();
    Serial.printf("sd_status=%s\n", card_mounted ? "mounted" : "not_mounted");
  }
  if (card_mounted && millis() - last_listing >= 15000) {
    last_listing = millis();
    printDirectory();
  }
  delay(50);
}
