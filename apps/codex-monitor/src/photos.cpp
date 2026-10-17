#include "monitor_photos.hpp"
#include "monitor_configuration.hpp"
#include <fnk0104b/monitor_photos.hpp>
#include <Arduino.h>
#include <atomic>
#include <algorithm>
namespace monitor_photos {
namespace {
constexpr char configUuid[] = "4e4b0104-0007-4d20-8f4b-0104b0000001";
constexpr char transferUuid[] = "4e4b0104-0008-4d20-8f4b-0104b0000001";
Preferences* prefs = nullptr;
ui::photos::Settings config;
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
uint8_t mailbox[200]; size_t mailboxSize = 0;
bool pending = false;
std::atomic<bool> disconnectPending{false};
char reply[480] = "{\"seq\":0,\"state\":\"ready\",\"offset\":0,\"files\":[]}";
bool dirty = false, active = false, idle = false, mounted = false;
uint16_t sequence = 0;
char names[8][25]{}, uploadName[25]{};
unsigned count = 0, index = 0;
uint32_t received = 0, expectedCrc = 0, crc = ~0u, lastPacket = 0, lastFrame = 0;
uint8_t uploadHeader[16]{};
uint16_t* frame = nullptr;
fs::File loading;
size_t loaded = 0;
uint32_t loadCrc = ~0u, frameCrc = 0;
bool frameReady = false;
bool serialReply = false;
uint32_t previewUntil = 0;
unsigned failedLoads = 0;
void enqueue(const uint8_t* data, size_t size) {
  if (size < 3 || size > sizeof(mailbox)) return;
  portENTER_CRITICAL(&mux);
  if (!pending) { memcpy(mailbox, data, size); mailboxSize = size; pending = true; }
  portEXIT_CRITICAL(&mux);
}
void report(const char* state) {
  char value[480];
  size_t n = snprintf(value, sizeof(value), "{\"seq\":%u,\"state\":\"%s\",\"offset\":%lu,\"mounted\":%s,\"files\":[",
      sequence, state, (unsigned long)received, mounted ? "true" : "false");
  for (unsigned i = 0; i < count; ++i)
    n += snprintf(value + n, sizeof(value) - n, "%s\"%s\"", i ? "," : "", names[i]);
  snprintf(value + n, sizeof(value) - n, "]}");
  portENTER_CRITICAL(&mux); strlcpy(reply, value, sizeof(reply)); portEXIT_CRITICAL(&mux);
  if (serialReply) { Serial.printf("monitor_photo_reply %s\n", value); serialReply = false; }
}
void rescan() {
  mounted = fnk0104b::monitor_photos::scan(names, count);
  index = 0; failedLoads = 0; dirty = true;
  Serial.printf("monitor_photos mounted=%u count=%u heap=%u psram=%u\n", mounted, count, unsigned(ESP.getFreeHeap()), unsigned(ESP.getFreePsram()));
}
void clearFrame() {
  loading.close();
  fnk0104b::monitor_photos::releaseFrame(frame); frame = nullptr;
  frameReady = false; loaded = 0;
}
class ConfigCallbacks : public BLECharacteristicCallbacks {
  void onRead(BLECharacteristic* c) override {
    const auto s = settings();
    uint8_t data[] = {1, s.mode, uint8_t(s.delay), uint8_t(s.delay >> 8), uint8_t(s.interval), uint8_t(s.interval >> 8)};
    c->setValue(data, sizeof(data));
  }
  void onWrite(BLECharacteristic* c) override {
    if (c->getLength() != 6) return;
    uint8_t data[9] = {10, 0, 0}; memcpy(data + 3, c->getData(), 6); enqueue(data, 9);
  }
};
class TransferCallbacks : public BLECharacteristicCallbacks {
  void onRead(BLECharacteristic* c) override {
    char value[480]; portENTER_CRITICAL(&mux); strlcpy(value, reply, sizeof(value)); portEXIT_CRITICAL(&mux);
    c->setValue(value);
  }
  void onWrite(BLECharacteristic* c) override { enqueue(c->getData(), c->getLength()); }
};
void process(const uint8_t* p, size_t size) {
  if (p[0] == 10) {
    ui::photos::Settings next;
    if (!ui::photos::decode(p + 3, size - 3, next)) return;
    if (!monitor_config::savePhotos(*prefs, next)) { report("settings_storage_failed"); return; }
    portENTER_CRITICAL(&mux); config = next; portEXIT_CRITICAL(&mux);
    clearFrame();
    if (idle && next.mode == 2) rescan();
    dirty = true;
    Serial.printf("monitor_photos settings mode=%u delay=%u interval=%u\n", next.mode, next.delay, next.interval);
    return;
  }
  sequence = p[1] | uint16_t(p[2]) << 8;
  lastPacket = millis();
  if (p[0] == 1) {
    if (active || size < 16 || size > 35 || ui::photos::le32(p + 3) != ui::photos::fileBytes) { report("invalid_begin"); return; }
    memcpy(uploadName, p + 11, size - 11); uploadName[size - 11] = 0;
    if (!ui::photos::validName(uploadName) || !fnk0104b::monitor_photos::beginUpload(uploadName)) { report("card_or_file_limit"); return; }
    mounted = true; expectedCrc = ui::photos::le32(p + 7); received = 0; crc = ~0u; active = true; report("receiving");
  } else if (p[0] == 2) {
    if (!active || size <= 7 || ui::photos::le32(p + 3) != received || received + size - 7 > ui::photos::fileBytes) { report("invalid_offset"); return; }
    const size_t bytes = size - 7;
    if (!fnk0104b::monitor_photos::write(p + 7, bytes)) { fnk0104b::monitor_photos::abort(); active = false; report("write_failed"); return; }
    if (received < 16) memcpy(uploadHeader + received, p + 7, std::min(bytes, size_t(16 - received)));
    crc = ui::photos::crcUpdate(crc, p + 7, bytes); received += bytes; report("receiving");
  } else if (p[0] == 3 && size == 3) {
    if (!active || received != ui::photos::fileBytes || (crc ^ ~0u) != expectedCrc || !ui::photos::validHeader(uploadHeader, received)) {
      fnk0104b::monitor_photos::abort(); active = false; report("checksum_or_size_failed"); return;
    }
    const bool ok = fnk0104b::monitor_photos::commit(uploadName); active = false;
    if (!ok) fnk0104b::monitor_photos::abort();
    clearFrame(); rescan(); report(ok ? "complete" : "commit_failed");
  } else if (p[0] == 4 && size == 3 && !active) { rescan(); report("ready"); }
  else if (p[0] == 5 && size > 7 && size <= 27 && !active) {
    char name[25]; memcpy(name, p + 3, size - 3); name[size - 3] = 0;
    clearFrame(); const bool ok = fnk0104b::monitor_photos::remove(name); rescan(); report(ok ? "deleted" : "delete_failed");
  } else if (p[0] == 6 && size == 3) { fnk0104b::monitor_photos::abort(); active = false; received = 0; report("cancelled"); }
  else report("invalid_command");
}
void startLoad() {
  if (!count || failedLoads >= count) return;
  if (!frame) frame = fnk0104b::monitor_photos::allocateFrame();
  if (!frame) { ++failedLoads; return; }
  loading = fnk0104b::monitor_photos::open(names[index]);
  uint8_t header[16];
  if (!loading || loading.read(header, sizeof(header)) != sizeof(header) || !ui::photos::validHeader(header, loading.size())) {
    loading.close(); index = (index + 1) % count; ++failedLoads; return;
  }
  frameCrc = ui::photos::le32(header + 12); loaded = 0; loadCrc = ~0u;
}
}
void begin(Preferences& preferences) {
  prefs = &preferences;
  config = monitor_config::loadPhotos(*prefs);
}
void attach(BLEService* service) {
  auto* c = service->createCharacteristic(configUuid, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
  c->setCallbacks(new ConfigCallbacks());
  c = service->createCharacteristic(transferUuid, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
  c->setCallbacks(new TransferCallbacks());
}
void disconnected() { disconnectPending = true; }
ui::photos::Settings settings() { ui::photos::Settings result; portENTER_CRITICAL(&mux); result = config; portEXIT_CRITICAL(&mux); return result; }
bool uploading() { return active; }
bool changed() { const bool result = dirty; dirty = false; return result; }
void setIdle(bool value) {
  if (idle == value) return;
  idle = value;
  if (!idle) clearFrame();
  else { rescan(); lastFrame = millis(); }
}
bool draw() { if (!frameReady) return false; fnk0104b::monitor_photos::drawFrame(frame); return true; }
void tick(bool allowed) {
  if (disconnectPending.exchange(false) || (active && millis() - lastPacket > 30000)) {
    fnk0104b::monitor_photos::abort(); active = false;
    portENTER_CRITICAL(&mux); pending = false; portEXIT_CRITICAL(&mux); report("cancelled");
  }
  if (!allowed) return;
  uint8_t data[200]; size_t size = 0;
  portENTER_CRITICAL(&mux);
  if (pending) { size = mailboxSize; memcpy(data, mailbox, size); pending = false; }
  portEXIT_CRITICAL(&mux);
  if (size) process(data, size);
  if (active || !idle || (settings().mode != 2 && !preview()) || !count) return;
  if (frameReady && millis() - lastFrame >= uint32_t(settings().interval) * 1000) {
    frameReady = false; index = (index + 1) % count;
  }
  if (!frameReady && !loading) startLoad();
  if (loading) {
    const size_t bytes = std::min(size_t(4096), size_t(ui::photos::pixelBytes - loaded));
    auto* target = reinterpret_cast<uint8_t*>(frame) + loaded;
    if (loading.read(target, bytes) != bytes) { loading.close(); ++failedLoads; index = (index + 1) % count; return; }
    loadCrc = ui::photos::crcUpdate(loadCrc, target, bytes); loaded += bytes;
    if (loaded == ui::photos::pixelBytes) {
      loading.close(); frameReady = (loadCrc ^ ~0u) == frameCrc;
      if (frameReady) { failedLoads = 0; lastFrame = millis(); dirty = true;
        Serial.printf("monitor_photos frame=%s heap=%u psram=%u\n", names[index], unsigned(ESP.getFreeHeap()), unsigned(ESP.getFreePsram())); }
      else { ++failedLoads; index = (index + 1) % count; }
    }
  }
}
void diagnostic(const char* command) {
  // Feed the same bounded mailbox used by BLE for reproducible SD/CRC diagnostics.
  if (!strncmp(command, "photo-packet ", 13)) {
    const char* hex = command + 13;
    const size_t length = strlen(hex);
    uint8_t packet[200];
    if (length < 6 || length > sizeof(packet) * 2 || length % 2) return;
    auto digit = [](char c) -> int { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1; };
    for (size_t i = 0; i < length / 2; ++i) {
      const int hi = digit(hex[i * 2]), lo = digit(hex[i * 2 + 1]);
      if (hi < 0 || lo < 0) return;
      packet[i] = uint8_t(hi * 16 + lo);
    }
    serialReply = packet[0] != 10;
    enqueue(packet, length / 2);
  }
  if (!strcmp(command, "photo-list")) { rescan(); report("ready"); Serial.println(reply); }
  if (!strcmp(command, "photo-preview")) { previewUntil = millis() + 30000; dirty = true; }
  if (!strcmp(command, "photo-off")) { cancelPreview(); setIdle(false); dirty = true; }
}
bool preview() { return previewUntil && static_cast<int32_t>(previewUntil - millis()) > 0; }
void cancelPreview() { previewUntil = 0; }
}
