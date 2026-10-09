#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <esp_crt_bundle.h>
#include <esp_http_client.h>
#include <esp_ota_ops.h>
#include <mbedtls/sha256.h>
#include <time.h>

#include <atomic>

#include "monitor_ota.hpp"
#include "monitor_speech.hpp"
#include "monitor_update_policy.hpp"

namespace monitor_ota {
namespace {
constexpr char base[] =
    "https://cmwen.github.io/FNK0104B/firmware/codex-monitor/";
constexpr char uuid[] = "4e4b0104-0005-4d20-8f4b-0104b0000001";
std::atomic<int> pending{0}, workAction{0};
std::atomic<bool> running{false};
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
char state[20] = "idle", message[120] = "Check for a compatible update",
     latest[32] = "";
int progress = 0;
std::atomic<bool> ready{false};
bool healthy = false, confirmed = false;
uint32_t started = 0;
struct Candidate {
  char version[32]{}, sha[65]{}, models[65]{};
  size_t size = 0, modelSize = 0;
} candidate;
void report(const char* s, const char* m, int p = 0) {
  portENTER_CRITICAL(&mux);
  strlcpy(state, s, sizeof(state));
  strlcpy(message, m, sizeof(message));
  progress = p;
  portEXIT_CRITICAL(&mux);
  Serial.printf("monitor_ota state=%s progress=%d message=%s\n", s, p, m);
}
String snapshot() {
  char s[20], m[120], v[32];
  int p;
  portENTER_CRITICAL(&mux);
  strlcpy(s, state, sizeof(s));
  strlcpy(m, message, sizeof(m));
  strlcpy(v, latest, sizeof(v));
  p = progress;
  portEXIT_CRITICAL(&mux);
  DynamicJsonDocument doc(1024);
  doc["state"] = s;
  doc["message"] = m;
  doc["current"] = version;
  doc["latest"] = v;
  doc["progress"] = p;
  String out;
  serializeJson(doc, out);
  return out;
}
class Callbacks : public BLECharacteristicCallbacks {
  void onRead(BLECharacteristic* c) override { c->setValue(snapshot()); }
  void onWrite(BLECharacteristic* c) override {
    request(c->getValue().c_str());
  }
};
void hexDigest(const uint8_t* bytes, char* out) {
  for (int i = 0; i < 32; ++i) snprintf(out + i * 2, 3, "%02x", bytes[i]);
}
struct Http {
  esp_http_client_handle_t handle = nullptr;
  ~Http() {
    if (handle) esp_http_client_cleanup(handle);
  }
  bool open(const char* url) {
    esp_http_client_config_t config{};
    config.url = url;
    config.crt_bundle_attach = esp_crt_bundle_attach;
    config.timeout_ms = 20000;
    config.buffer_size = 4096;
    config.buffer_size_tx = 1024;
    config.disable_auto_redirect =
        true;  // Published files live on this fixed HTTPS origin.
    handle = esp_http_client_init(&config);
    return handle && esp_http_client_open(handle, 0) == ESP_OK &&
           esp_http_client_fetch_headers(handle) >= 0 &&
           esp_http_client_get_status_code(handle) == 200;
  }
};
bool modelMatches() {
  const auto* part = esp_partition_find_first(
      ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "model");
  if (!part || part->address != 0x810000 || !candidate.modelSize ||
      candidate.modelSize > part->size)
    return false;
  auto* buffer = static_cast<uint8_t*>(ps_malloc(4096));
  if (!buffer) return false;
  mbedtls_sha256_context ctx;
  mbedtls_sha256_init(&ctx);
  mbedtls_sha256_starts(&ctx, 0);
  bool ok = true;
  for (size_t offset = 0; offset < candidate.modelSize; offset += 4096) {
    size_t n = min<size_t>(4096, candidate.modelSize - offset);
    if (esp_partition_read(part, offset, buffer, n) != ESP_OK ||
        mbedtls_sha256_update(&ctx, buffer, n) != 0) {
      ok = false;
      break;
    }
    delay(1);
  }
  uint8_t digest[32];
  char hex[65];
  mbedtls_sha256_finish(&ctx, digest);
  hexDigest(digest, hex);
  mbedtls_sha256_free(&ctx);
  free(buffer);
  return ok && !strcmp(hex, candidate.models);
}
bool check() {
  ready = false;
  report("checking", "Checking published monitor firmware");
  Http http;
  String url = String(base) + "ota.json";
  if (!http.open(url.c_str())) {
    report("error", "Update catalog unavailable; try again later");
    return false;
  }
  char json[1025];
  int used = 0, n;
  while (used < 1024 &&
         (n = esp_http_client_read(http.handle, json + used, 1024 - used)) > 0)
    used += n;
  if (!esp_http_client_is_complete_data_received(http.handle)) {
    report("error", "Invalid update catalog length");
    return false;
  }
  json[used] = 0;
  DynamicJsonDocument doc(1024);
  if (deserializeJson(doc, json) || doc["schema"].as<int>() != 1 ||
      strcmp(doc["project"] | "", "codex-monitor") ||
      strcmp(doc["layout"] | "", layout)) {
    report("usb_required", "Published firmware needs a USB installation");
    return false;
  }
  strlcpy(candidate.version, doc["version"] | "", sizeof(candidate.version));
  strlcpy(candidate.sha, doc["sha256"] | "", sizeof(candidate.sha));
  strlcpy(candidate.models, doc["models_sha256"] | "",
          sizeof(candidate.models));
  candidate.size = doc["size"] | 0U;
  candidate.modelSize = doc["models_size"] | 0U;
  portENTER_CRITICAL(&mux);
  strlcpy(latest, candidate.version, sizeof(latest));
  portEXIT_CRITICAL(&mux);
  const auto* next = esp_ota_get_next_update_partition(nullptr);
  if (!next || next->size != 0x400000 || !candidate.size ||
      candidate.size > next->size ||
      !monitor_update_policy::hashValid(candidate.sha) ||
      !monitor_update_policy::hashValid(candidate.models)) {
    report("error", "Invalid update metadata");
    return false;
  }
  if (!monitor_update_policy::newer(candidate.version, version)) {
    report("current", "Installed firmware is current or newer");
    return false;
  }
  if (!modelMatches()) {
    report("usb_required",
           "Speech models changed; install all images over USB");
    return false;
  }
  ready = true;
  report("available", "Compatible update ready; choose Install update");
  return true;
}
bool install() {
  // Recheck immediately before writing: a Pages deployment may have changed.
  Candidate approved = candidate;
  if (!check()) return false;
  if (strcmp(approved.version, candidate.version) ||
      strcmp(approved.sha, candidate.sha)) {
    report("available",
           "Published update changed; review it and choose Install again");
    return false;
  }
  ready = false;
  report("installing", "Downloading update; microphone is off");
  const auto* next = esp_ota_get_next_update_partition(nullptr);
  Http http;
  String url = String(base) + "firmware.bin";
  if (!http.open(url.c_str()) || esp_http_client_get_content_length(
                                     http.handle) != int64_t(candidate.size)) {
    report("error", "Download unavailable or size changed; check again");
    return false;
  }
  auto* buffer = static_cast<uint8_t*>(ps_malloc(4096));
  if (!buffer) {
    report("error", "Update buffer unavailable");
    return false;
  }
  esp_ota_handle_t handle = 0;
  if (esp_ota_begin(next, candidate.size, &handle) != ESP_OK) {
    free(buffer);
    report("error", "Cannot open inactive firmware slot");
    return false;
  }
  mbedtls_sha256_context ctx;
  mbedtls_sha256_init(&ctx);
  mbedtls_sha256_starts(&ctx, 0);
  size_t total = 0;
  int last = -1;
  bool ok = true;
  while (total < candidate.size) {
    int n = esp_http_client_read(http.handle, reinterpret_cast<char*>(buffer),
                                 min<size_t>(4096, candidate.size - total));
    if (n <= 0 || esp_ota_write(handle, buffer, n) != ESP_OK ||
        mbedtls_sha256_update(&ctx, buffer, n) != 0) {
      ok = false;
      break;
    }
    total += n;
    int p = total * 100 / candidate.size;
    if (p >= last + 5) {
      report("installing", "Downloading update; microphone is off", p);
      last = p;
    }
    delay(1);
  }
  uint8_t digest[32];
  char hex[65];
  mbedtls_sha256_finish(&ctx, digest);
  hexDigest(digest, hex);
  mbedtls_sha256_free(&ctx);
  free(buffer);
  if (!ok || total != candidate.size || strcmp(hex, candidate.sha) ||
      !esp_http_client_is_complete_data_received(http.handle)) {
    esp_ota_abort(handle);
    report("error", "Incomplete or changed download; current firmware kept");
    return false;
  }
  if (esp_ota_end(handle) != ESP_OK) {
    report("error", "Firmware validation failed; current firmware kept");
    return false;
  }
  // Verify app identity/version after the IDF image validation and before
  // selection.
  esp_app_desc_t desc{};
  if (esp_ota_get_partition_description(next, &desc) != ESP_OK ||
      strcmp(desc.project_name, "fnk0104b_firmware") ||
      strcmp(desc.version, candidate.version) ||
      esp_ota_set_boot_partition(next) != ESP_OK) {
    report("error", "Firmware identity or boot selection failed");
    return false;
  }
  report("restarting", "Update verified; restarting board", 100);
  delay(1500);
  ESP.restart();
  return true;
}
void worker(int action) {
  uint32_t quietStart = millis();
  while (!monitor_speech::paused() && millis() - quietStart < 5000) delay(20);
  if (!monitor_speech::paused())
    report("error", "Speech worker did not pause; try again");
  else if (WiFi.status() != WL_CONNECTED)
    report("error", "Connect the board to Wi-Fi first");
  else {
    configTime(0, 0, "pool.ntp.org", "time.cloudflare.com");
    uint32_t start = millis();
    while (time(nullptr) < 1700000000 && millis() - start < 30000) delay(250);
    if (time(nullptr) < 1700000000)
      report("error", "Clock sync failed; HTTPS requires a valid clock");
    else if (action == 2)
      install();
    else
      check();
  }
  running = false;
}
}  // namespace
void attach(BLEService* service) {
  auto* c = service->createCharacteristic(
      uuid,
      BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
  c->setCallbacks(new Callbacks());
  c->setValue(snapshot());
}
void begin(bool coreReady) {
  healthy = coreReady;
  started = millis();
}
void tick() {
  if (!confirmed && healthy && millis() - started >= 10000) {
    const auto* part = esp_ota_get_running_partition();
    esp_ota_img_states_t status;
    if (part && esp_ota_get_state_partition(part, &status) == ESP_OK &&
        status == ESP_OTA_IMG_PENDING_VERIFY) {
      if (esp_ota_mark_app_valid_cancel_rollback() != ESP_OK) return;
      Serial.println("monitor_ota boot=confirmed");
    }
    confirmed = true;
  }
}
void request(const char* command) {
  int action =
      !strcmp(command, "check") ? 1 : (!strcmp(command, "install") ? 2 : 0);
  if (!action || running.load()) return;
  if (action == 2 && !ready) return;
  int empty = 0;
  pending.compare_exchange_strong(empty, action);
}
bool requested() { return pending.load() != 0; }
bool busy() { return running.load(); }
bool runPending() {
  const int action = workAction.exchange(0);
  if (!action) return false;
  worker(action);
  return true;
}
void start(bool allowed, TaskHandle_t workerTask) {
  int action = pending.exchange(0);
  if (!action) return;
  if (!allowed) {
    report("error", "Finish voice activity before checking or installing");
    return;
  }
  if (!workerTask) {
    report("error", "Update worker unavailable");
    return;
  }
  report("checking", "Preparing update check; microphone is off");
  running = true;
  workAction = action;
  xTaskNotifyGive(workerTask);
}
}  // namespace monitor_ota
