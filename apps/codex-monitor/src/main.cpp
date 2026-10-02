#include <Arduino.h>
#include <ArduinoJson.h>
#include <BLE2902.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WiFi.h>

#include <fnk0104b/board.hpp>
#include <fnk0104b/pins.hpp>
#include <locallink/protocol.hpp>
#include <locallink/sse.hpp>
#include <ui/avatar_assets.hpp>
#include <ui/monitor_theme.hpp>
#include <ui/idle_timer.hpp>

#include <atomic>
#include <errno.h>
#include <memory>
#include <new>
#include <sys/select.h>
#include <sys/socket.h>
#include <stdio.h>
#include <string.h>

#if __has_include("monitor_secrets.h")
#include "monitor_secrets.h"
#else
#define MONITOR_SERVER_HOST ""
#define MONITOR_SERVER_PORT 8765
#define MONITOR_SERVER_TOKEN ""
#endif

namespace {
constexpr uint32_t kStatusTimeoutMs = 15000;
constexpr uint32_t kStreamSilenceMs = 90000;
constexpr uint32_t kAvatarFrameMs = 100;
constexpr uint32_t kScreenTimeoutDefault = 30;
constexpr uint32_t kSampleRate = 16000;
constexpr uint32_t kRecordSeconds = 9;
constexpr size_t kRecordSamples = kSampleRate * kRecordSeconds;
constexpr size_t kMaxStatusBytes = 8192;
constexpr char kServiceUuid[] = "4e4b0104-0001-4d20-8f4b-0104b0000001";
constexpr char kCharacteristicUuid[] = "4e4b0104-0002-4d20-8f4b-0104b0000001";
constexpr uint16_t kBg = ui::monitor::kBackground;
constexpr uint16_t kPanel = ui::monitor::kPanel;
constexpr uint16_t kText = ui::monitor::kText;
constexpr uint16_t kMuted = ui::monitor::kMuted;
constexpr uint16_t kGreen = ui::monitor::kMint;
constexpr uint16_t kAmber = ui::monitor::kAmber;
constexpr uint16_t kRed = ui::monitor::kRed;
constexpr uint16_t kBlue = ui::monitor::kCyan;

struct Quota { int16_t used = -1; int64_t resets = -1; };
struct Agent { char id[80]{}; char name[48]{}; char status[20]{}; char detail[120]{}; };
struct MonitorStatus {
  char integration[20] = "unavailable";
  Quota five_hour, weekly;
  Agent agents[8];
  uint8_t count = 0;
  uint16_t total_agents = 0;
};

MonitorStatus status;
portMUX_TYPE statusMux = portMUX_INITIALIZER_UNLOCKED;
Preferences preferences;
std::atomic<uint8_t> volumePercent{50};
std::atomic<uint16_t> screenTimeoutMinutes{kScreenTimeoutDefault};
ui::IdleTimer idleTimer;
uint32_t lastAvatarFrame = 0;
bool screenAwake = true, previousTouch = false;
bool bleConnected = false;
std::atomic<bool> recording{false};
std::atomic<bool> voiceBusy{false};
std::atomic<bool> voicePreparing{false};
std::atomic<bool> uiDirty{true};
std::atomic<bool> statusPaused{false};
std::atomic<bool> refreshOnWake{false};
bool previousMessageVisible = false;
bool previousWifiConnected = false;
std::atomic<bool> pendingAttentionTone{false};
uint32_t previousAttentionIds[50] = {};
uint8_t previousAttentionCount = 0;
char selectedAgent[80] = {};
Agent selectedDetail{};
char voiceAgent[80] = {};
char transientMessage[48] = "Starting";
uint32_t messageUntil = 0;
portMUX_TYPE messageMux = portMUX_INITIALIZER_UNLOCKED;
TaskHandle_t voiceTask = nullptr;
TaskHandle_t statusTask = nullptr;
std::atomic<bool> stopCapture{false};
fnk0104b::TouchPoint touchPoint{0, 0, false};
ui::avatar::Canvas avatarCanvas;
uint16_t avatarScaled[96 * 96];
BLECharacteristic* settingsCharacteristic = nullptr;
SemaphoreHandle_t peripheralMutex = nullptr;
SemaphoreHandle_t streamMutex = nullptr;
int statusSocket = -1;  // Guarded by streamMutex; only the worker closes it.

TFT_eSPI& tft() { return fnk0104b::display.driver(); }

void setMessage(const char* text, uint32_t duration = 2500) {
  portENTER_CRITICAL(&messageMux);
  strlcpy(transientMessage, text, sizeof(transientMessage));
  messageUntil = millis() + duration;
  portEXIT_CRITICAL(&messageMux);
  uiDirty = true;
}

String encodeQueryValue(const char* value) {
  static const char hex[] = "0123456789ABCDEF";
  String encoded;
  if (!value) return encoded;
  for (const uint8_t* p = reinterpret_cast<const uint8_t*>(value); *p; ++p) {
    const uint8_t c = *p;
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
      encoded += static_cast<char>(c);
    } else {
      encoded += '%'; encoded += hex[c >> 4]; encoded += hex[c & 0x0f];
    }
  }
  return encoded;
}

void markOffline() {
  bool wasOnline;
  portENTER_CRITICAL(&statusMux);
  wasOnline = strcmp(status.integration, "unavailable") != 0;
  strlcpy(status.integration, "unavailable", sizeof(status.integration));
  status.five_hour = Quota{};
  status.weekly = Quota{};
  status.count = 0;
  status.total_agents = 0;
  previousAttentionCount = 0;
  portEXIT_CRITICAL(&statusMux);
  if (wasOnline) {
    Serial.println("monitor_status integration=unavailable");
    uiDirty = true;
  }
}

bool sameAgent(const Agent& left, const Agent& right) {
  return !strcmp(left.id, right.id) && !strcmp(left.name, right.name) &&
         !strcmp(left.status, right.status) && !strcmp(left.detail, right.detail);
}

bool sameStatus(const MonitorStatus& left, const MonitorStatus& right) {
  if (strcmp(left.integration, right.integration) ||
      left.five_hour.used != right.five_hour.used ||
      left.weekly.used != right.weekly.used ||
      left.count != right.count || left.total_agents != right.total_agents) return false;
  for (uint8_t index = 0; index < left.count && index < 4; ++index)
    if (!sameAgent(left.agents[index], right.agents[index])) return false;
  return true;
}

uint16_t agentColor(const char* state) {
  if (!strcmp(state, "needs_attention")) return kAmber;
  if (!strcmp(state, "error")) return kRed;
  return kGreen;
}

const char* agentLabel(const char* state) {
  return !strcmp(state, "needs_attention") ? "Needs input" :
      (!strcmp(state, "error") ? "Error" : "Working");
}

void drawAvatar(const Agent& a, int x, int y, int size, bool drawBorder = true) {
  const ui::avatar::Mood mood = !strcmp(a.status, "error")
      ? ui::avatar::Mood::Error
      : (!strcmp(a.status, "needs_attention") ? ui::avatar::Mood::NeedsInput
                                                : ui::avatar::Mood::Thinking);
  ui::avatar::render(avatarCanvas, ui::avatar::hashId(a.id), mood,
                     millis() / kAvatarFrameMs);
  for (int row = 0; row < size; ++row) {
    const int sourceRow = row * 32 / size;
    for (int column = 0; column < size; ++column)
      avatarScaled[row * size + column] =
          avatarCanvas.pixels[sourceRow * 32 + column * 32 / size];
  }
  TFT_eSPI& d = tft();
  if (drawBorder)
    d.fillRoundRect(x - 3, y - 3, size + 6, size + 6, 6, agentColor(a.status));
  d.setSwapBytes(true);
  d.pushImage(x, y, size, size, avatarScaled);
  d.setSwapBytes(false);
}

String shortText(const char* value, size_t maximum) {
  String result(value);
  if (result.length() > maximum) result = result.substring(0, maximum - 3) + "...";
  return result;
}

void drawAgentTile(const Agent& agent, int x, int y) {
  TFT_eSPI& d = tft();
  ui::monitor::frame(d, x, y, 148, 61, agentColor(agent.status));
  drawAvatar(agent, x + 5, y + 6, 48);
  d.setTextColor(kText, kPanel);
  d.drawString(shortText(agent.name, 11), x + 70, y + 12, 1);
  d.setTextColor(agentColor(agent.status), kPanel);
  d.drawString(!strcmp(agent.status, "needs_attention") ? "NEEDS INPUT" :
      (!strcmp(agent.status, "error") ? "ERROR" : "RUNNING"), x + 70, y + 34, 1);
}

void drawAnimatedAvatars() {
  MonitorStatus snapshot;
  portENTER_CRITICAL(&statusMux); snapshot = status; portEXIT_CRITICAL(&statusMux);
  if (!snapshot.count) return;
  if (selectedAgent[0]) drawAvatar(selectedDetail, 18, 74, 96, false);
  else if (snapshot.count == 1) drawAvatar(snapshot.agents[0], 18, 74, 96, false);
  else for (uint8_t i = 0; i < snapshot.count && i < 4; ++i)
    drawAvatar(snapshot.agents[i], 13 + (i % 2) * 156, 65 + (i / 2) * 64, 48, false);
}

void drawScreen() {
  if (!screenAwake) return;
  TFT_eSPI& d = tft();
  MonitorStatus snapshot;
  portENTER_CRITICAL(&statusMux); snapshot = status; portEXIT_CRITICAL(&statusMux);
  d.fillScreen(kBg);
  bool attention = false, error = false;
  for (uint8_t i = 0; i < snapshot.count; ++i) {
    attention |= !strcmp(snapshot.agents[i].status, "needs_attention");
    error |= !strcmp(snapshot.agents[i].status, "error");
  }
  const bool wifi = WiFi.status() == WL_CONNECTED;
  ui::monitor::statusBar(d, wifi, wifi ? WiFi.RSSI() : -127,
      snapshot.integration, snapshot.count > 0, attention, error,
      ui::monitor::remaining(snapshot.five_hour.used), ui::monitor::remaining(snapshot.weekly.used));
  char message[sizeof(transientMessage)];
  uint32_t messageExpiry;
  portENTER_CRITICAL(&messageMux);
  strlcpy(message, transientMessage, sizeof(message));
  messageExpiry = messageUntil;
  portEXIT_CRITICAL(&messageMux);
  const bool messageVisible = static_cast<int32_t>(messageExpiry - millis()) > 0;
  const bool showingSelected = selectedAgent[0] != '\0';
  d.setTextColor(kMuted, kBg);
  if (messageVisible) d.drawString(shortText(message, 48), 10, 45, 1);
  else if (snapshot.count) d.drawString(showingSelected ? "AGENT DETAIL / QUOTA LEFT" : "ACTIVE AGENTS / QUOTA LEFT", 10, 45, 1);
  else d.drawString(!strcmp(snapshot.integration, "connected") ?
      "QUOTA LEFT" : "BRIDGE UNAVAILABLE", 10, 45, 1);
  if (snapshot.count == 0) {
    ui::monitor::quotaCard(d, 6, "5H left", ui::monitor::remaining(snapshot.five_hour.used), false);
    ui::monitor::quotaCard(d, 164, "Week left", ui::monitor::remaining(snapshot.weekly.used), true);
  } else if (showingSelected) {
    ui::monitor::frame(d, 8, 59, 304, 123, ui::monitor::kBorder);
    drawAvatar(selectedDetail, 18, 74, 96);
    d.setTextColor(kText, kPanel);
    d.drawString(shortText(selectedDetail.name, 15), 128, 70, 2);
    d.setTextColor(agentColor(selectedDetail.status), kPanel);
    d.drawString(agentLabel(selectedDetail.status), 128, 96, 1);
    d.setTextColor(kText, kPanel);
    String detail(selectedDetail.detail);
    d.drawString(detail.substring(0, 29), 128, 116, 1);
    if (detail.length() > 29) d.drawString(detail.substring(29, 58), 128, 130, 1);
    if (detail.length() > 58) d.drawString(detail.substring(58, 87), 128, 144, 1);
    d.setTextColor(kMuted, kPanel);
    d.drawString("Tap panel to return", 128, 167, 1);
  } else if (snapshot.count == 1) {
    const Agent& agent = snapshot.agents[0];
    ui::monitor::frame(d, 8, 59, 304, 123, ui::monitor::kBorder);
    drawAvatar(agent, 18, 74, 96);
    d.setTextColor(kText, kPanel);
    d.drawString(shortText(agent.name, 15), 128, 70, 2);
    d.setTextColor(agentColor(agent.status), kPanel);
    d.drawString(agentLabel(agent.status), 128, 96, 1);
    d.setTextColor(kText, kPanel);
    d.drawString(shortText(agent.detail, 28), 128, 122, 1);
    if (!strcmp(agent.status, "needs_attention")) {
      d.setTextColor(kMuted, kPanel);
      d.drawString("Tap avatar for detail", 128, 162, 1);
    }
  } else {
    for (uint8_t i = 0; i < snapshot.count && i < 4; ++i)
      drawAgentTile(snapshot.agents[i], 8 + (i % 2) * 156, 59 + (i / 2) * 64);
    if (snapshot.total_agents > 4) {
      d.setTextColor(kMuted, kBg);
      d.drawRightString(String("+") + String(snapshot.total_agents - 4) + " more", 310, 45, 1);
    }
  }
  ui::monitor::voiceControl(d, recording.load(), voicePreparing.load(), voiceBusy.load(), selectedAgent[0] != '\0');
}

void publishSettings() {
  if (!settingsCharacteristic) return;
  uint8_t bytes[4] = {1, volumePercent, static_cast<uint8_t>(screenTimeoutMinutes & 0xff),
                      static_cast<uint8_t>(screenTimeoutMinutes >> 8)};
  settingsCharacteristic->setValue(bytes, sizeof(bytes));
}

class SettingsCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* characteristic) override {
    const std::string raw = characteristic->getValue();
    if (raw.size() != 4 || static_cast<uint8_t>(raw[0]) != 1) return;
    const uint8_t volume = static_cast<uint8_t>(raw[1]);
    const uint16_t timeout = static_cast<uint8_t>(raw[2]) |
                             (static_cast<uint16_t>(static_cast<uint8_t>(raw[3])) << 8);
    if (volume > 100 || timeout < 1 || timeout > 120) return;
    volumePercent = volume; screenTimeoutMinutes = timeout;
    preferences.putUChar("volume", volumePercent);
    preferences.putUShort("timeout", screenTimeoutMinutes);
    publishSettings();
  }
};

class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer*) override {
    bleConnected = true;
    Serial.println("monitor_ble connected");
  }
  void onDisconnect(BLEServer*) override {
    bleConnected = false;
    Serial.println("monitor_ble disconnected restarting_advertising");
    BLEDevice::startAdvertising();
  }
};

void bleGapDiagnostic(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t* param) {
  if (event == ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT)
    Serial.printf("monitor_ble advertising_data status=%d\n", param->adv_data_raw_cmpl.status);
  else if (event == ESP_GAP_BLE_SCAN_RSP_DATA_RAW_SET_COMPLETE_EVT)
    Serial.printf("monitor_ble scan_response status=%d\n", param->scan_rsp_data_raw_cmpl.status);
  else if (event == ESP_GAP_BLE_ADV_START_COMPLETE_EVT)
    Serial.printf("monitor_ble advertising_started status=%d\n", param->adv_start_cmpl.status);
}

void startBle() {
  BLEDevice::init("FNK0104B-MONITOR");
  BLEDevice::setCustomGapHandler(bleGapDiagnostic);
  BLEServer* server = BLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());
  BLEService* service = server->createService(kServiceUuid);
  settingsCharacteristic = service->createCharacteristic(kCharacteristicUuid,
      BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
  settingsCharacteristic->setCallbacks(new SettingsCallbacks());
  settingsCharacteristic->addDescriptor(new BLE2902());
  publishSettings(); service->start();
  BLEAdvertising* advertising = BLEDevice::getAdvertising();
  // Keep the full name in the primary advertisement for browser name filters.
  // Name + 128-bit service UUID cannot fit together in a legacy 31-byte packet.
  BLEAdvertisementData advertisement;
  advertisement.setFlags(ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT);
  advertisement.setName("FNK0104B-MONITOR");
  BLEAdvertisementData response;
  response.setCompleteServices(BLEUUID(kServiceUuid));
  advertising->setAdvertisementData(advertisement);
  advertising->setScanResponseData(response);
  advertising->setScanResponse(true);
  BLEDevice::startAdvertising();
}

void parseQuota(JsonVariantConst value, Quota& quota) {
  if (!value.is<JsonObjectConst>()) return;
  JsonVariantConst used = value["used_percent"];
  JsonVariantConst resets = value["resets_at"];
  if (used.is<int>() && used.as<int>() >= 0 && used.as<int>() <= 100) quota.used = used.as<int>();
  if (resets.is<int64_t>() && resets.as<int64_t>() >= 0) quota.resets = resets.as<int64_t>();
}

bool applyStatusJson(const char* body, size_t bodyLength) {
  DynamicJsonDocument doc(8192);
  const DeserializationError parseError = deserializeJson(doc, body, bodyLength);
  if (parseError || !doc.is<JsonObject>()) {
    Serial.printf("monitor_stream error=invalid_json detail=%s\n", parseError.c_str());
    return false;
  }
  MonitorStatus next;
  const char* integration = doc["integration"] | "unavailable";
  if (strcmp(integration, "connected") && strcmp(integration, "degraded") && strcmp(integration, "unavailable")) integration = "unavailable";
  strlcpy(next.integration, integration, sizeof(next.integration));
  parseQuota(doc["codex"]["usage"]["five_hour"], next.five_hour);
  parseQuota(doc["codex"]["usage"]["weekly"], next.weekly);
  JsonArrayConst agents = doc["agents"].as<JsonArrayConst>();
  uint32_t currentAttentionIds[50] = {};
  uint8_t currentAttentionCount = 0;
  for (JsonVariantConst item : agents) {
    if (!item.is<JsonObjectConst>()) continue;
    const char* state = item["status"] | "";
    if (strcmp(state, "running") && strcmp(state, "needs_attention") && strcmp(state, "error")) continue;
    const char* id = item["id"] | "";
    if (!strcmp(state, "needs_attention") && currentAttentionCount < 50) {
      const uint32_t identity = ui::avatar::hashId(id);
      bool duplicate = false;
      for (uint8_t n = 0; n < currentAttentionCount; ++n)
        if (currentAttentionIds[n] == identity) duplicate = true;
      if (!duplicate) currentAttentionIds[currentAttentionCount++] = identity;
    }
    ++next.total_agents;
    if (next.count >= 8) continue;
    Agent& a = next.agents[next.count++];
    strlcpy(a.id, id, sizeof(a.id));
    strlcpy(a.name, item["name"] | "Agent", sizeof(a.name));
    strlcpy(a.status, state, sizeof(a.status));
    strlcpy(a.detail, item["detail"] | "", sizeof(a.detail));
  }
  const int reportedTotal = doc["total_agents"] | static_cast<int>(next.total_agents);
  if (reportedTotal >= next.total_agents && reportedTotal <= 65535)
    next.total_agents = static_cast<uint16_t>(reportedTotal);
  MonitorStatus previous;
  portENTER_CRITICAL(&statusMux);
  previous = status;
  status = next;
  portEXIT_CRITICAL(&statusMux);
  const bool statusChanged = !sameStatus(previous, next);
  if (statusChanged) {
    uiDirty = true;
    Serial.printf("monitor_status integration=%s agents=%u first_state=%s five_hour_used=%d weekly_used=%d\n",
                  next.integration, next.total_agents, next.count ? next.agents[0].status : "none",
                  next.five_hour.used, next.weekly.used);
  }
  for (uint8_t i = 0; i < currentAttentionCount; ++i) {
    bool existed = false;
    for (uint8_t j = 0; j < previousAttentionCount; ++j)
      if (currentAttentionIds[i] == previousAttentionIds[j]) existed = true;
    if (!existed) { pendingAttentionTone = true; setMessage("Agent needs your attention"); break; }
  }
  memcpy(previousAttentionIds, currentAttentionIds, sizeof(currentAttentionIds));
  previousAttentionCount = currentAttentionCount;
  return true;
}

bool streamStatus() {
  if (!MONITOR_SERVER_HOST[0] || WiFi.status() != WL_CONNECTED) return false;
  std::unique_ptr<locallink::StatusEventParser<kMaxStatusBytes>> parser(
      new (std::nothrow) locallink::StatusEventParser<kMaxStatusBytes>());
  if (!parser) return false;
  WiFiClient client;
  HTTPClient http;
  String url = String("http://") + MONITOR_SERVER_HOST + ":" + String(MONITOR_SERVER_PORT) + "/v1/events";
  const bool refresh = refreshOnWake.exchange(false);
  if (refresh) url += "?refresh=1";
  http.setConnectTimeout(5000);
  http.setTimeout(kStatusTimeoutMs);
  const char* headers[] = {"Content-Type", "Transfer-Encoding"};
  if (!http.begin(client, url)) { if (refresh) refreshOnWake = true; return false; }
  http.collectHeaders(headers, 2);
  http.addHeader("Accept", "text/event-stream");
  if (MONITOR_SERVER_TOKEN[0]) http.addHeader("X-Monitor-Key", MONITOR_SERVER_TOKEN);
  const int code = http.GET();
  if (code != HTTP_CODE_OK || !http.header("Content-Type").startsWith("text/event-stream") ||
      http.header("Transfer-Encoding").length()) {
    Serial.printf("monitor_stream error=http code=%d\n", code);
    if (refresh) refreshOnWake = true;
    http.end();
    return false;
  }
  xSemaphoreTake(streamMutex, portMAX_DELAY);
  statusSocket = client.fd();
  xSemaphoreGive(streamMutex);
  Serial.println("monitor_stream state=connected");
  bool receivedStatus = false;
  uint32_t lastByte = millis();
  uint8_t bytes[1024];
  while (!statusPaused.load()) {
    // HTTP header parsing may have prefetched body bytes into WiFiClient.
    // Guard calls that can close its descriptor against the UI's shutdown.
    xSemaphoreTake(streamMutex, portMAX_DELAY);
    const int available = client.available();
    const int count = available > 0 ? client.read(bytes, min(available, static_cast<int>(sizeof(bytes)))) : 0;
    const bool connected = count > 0 || client.connected();
    const int fd = client.fd();
    statusSocket = fd;
    xSemaphoreGive(streamMutex);
    if (!connected || fd < 0) break;
    if (count > 0) {
      lastByte = millis();
      bool invalid = false;
      for (int i = 0; i < count && !statusPaused.load(); ++i) {
        const auto result = parser->feed(static_cast<char>(bytes[i]));
        if (result == locallink::StatusEventParser<kMaxStatusBytes>::Result::Overflow) {
          Serial.println("monitor_stream error=event_too_large");
          invalid = true; break;
        }
        if (result == locallink::StatusEventParser<kMaxStatusBytes>::Result::Status) {
          if (!applyStatusJson(parser->data(), parser->size())) { invalid = true; break; }
          receivedStatus = true;
        }
      }
      if (invalid) break;
      continue;
    }
    const uint32_t elapsed = millis() - lastByte;
    if (elapsed >= kStreamSilenceMs) {
      Serial.println("monitor_stream error=heartbeat_timeout");
      break;
    }
    const uint32_t remaining = kStreamSilenceMs - elapsed;
    timeval timeout{static_cast<long>(remaining / 1000), static_cast<long>((remaining % 1000) * 1000)};
    fd_set readable;
    FD_ZERO(&readable); FD_SET(fd, &readable);
    // Blocking select sleeps until data, heartbeat, or quiet-mode shutdown.
    const int ready = select(fd + 1, &readable, nullptr, nullptr, &timeout);
    if (ready < 0 && errno != EINTR) break;
  }
  xSemaphoreTake(streamMutex, portMAX_DELAY);
  statusSocket = -1;
  xSemaphoreGive(streamMutex);
  http.end();
  Serial.printf("monitor_stream state=disconnected reason=%s\n", statusPaused.load() ? "quiet" : "link");
  return receivedStatus;
}

void statusWorker(void*) {
  uint32_t retryMs = 5000;
  for (;;) {
    while (statusPaused.load()) {
      retryMs = 5000;
      ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    }
    if (streamStatus()) retryMs = 5000;
    if (statusPaused.load()) continue;
    markOffline();
    Serial.printf("monitor_stream state=retry wait_ms=%lu\n", static_cast<unsigned long>(retryMs));
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(retryMs));
    retryMs = min<uint32_t>(retryMs * 2, 60000);
  }
}

void pauseStatus() {
  statusPaused = true;
  xSemaphoreTake(streamMutex, portMAX_DELAY);
  if (statusSocket >= 0) shutdown(statusSocket, SHUT_RDWR);
  xSemaphoreGive(streamMutex);
  if (statusTask) xTaskNotifyGive(statusTask);
}

void resumeStatus() {
  refreshOnWake = true;
  statusPaused = false;
  if (statusTask) xTaskNotifyGive(statusTask);
}

// Selection and display state stay on the UI task; SSE publishes snapshots.
void refreshSelectedAgent() {
  if (!selectedAgent[0]) return;
  MonitorStatus snapshot;
  portENTER_CRITICAL(&statusMux); snapshot = status; portEXIT_CRITICAL(&statusMux);
  for (uint8_t i = 0; i < snapshot.count; ++i) {
    if (strcmp(snapshot.agents[i].id, selectedAgent)) continue;
    if (!sameAgent(selectedDetail, snapshot.agents[i])) {
      selectedDetail = snapshot.agents[i];
      uiDirty = true;
    }
    return;
  }
  selectedAgent[0] = '\0';
  selectedDetail = Agent{};
  uiDirty = true;
}

void playPendingAttentionTone() {
  if (!pendingAttentionTone || voiceBusy.load() || !peripheralMutex) return;
  pendingAttentionTone = false;
  xSemaphoreTake(peripheralMutex, portMAX_DELAY);
  if (fnk0104b::speaker.begin()) {
    fnk0104b::speaker.setVolume(volumePercent);
    fnk0104b::speaker.setTone(880);
    delay(220);
    fnk0104b::speaker.setTone(0);
    fnk0104b::speaker.end();
    Serial.println("monitor_attention_tone played");
  } else {
    Serial.println("monitor_attention_tone unavailable");
  }
  xSemaphoreGive(peripheralMutex);
}

bool captureStop(void*) { return stopCapture.load(std::memory_order_relaxed); }

void voiceWorker(void*) {
  int16_t* pcm = nullptr;
  uint8_t* wav = nullptr;
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    if (WiFi.status() != WL_CONNECTED || !MONITOR_SERVER_HOST[0]) {
      voicePreparing = false;
      recording = false; voiceBusy = false; uiDirty = true;
      setMessage("Bridge unavailable"); continue;
    }
    if (peripheralMutex) xSemaphoreTake(peripheralMutex, portMAX_DELAY);
    if (fnk0104b::speaker.ready()) fnk0104b::speaker.end();
    const bool micReady = fnk0104b::microphone.begin(fnk0104b::MicrophoneConfig{
        fnk0104b::pins::audio::i2s_master_clock, fnk0104b::pins::audio::i2s_bit_clock,
        fnk0104b::pins::audio::i2s_word_select, fnk0104b::pins::audio::i2s_data_out,
        fnk0104b::pins::audio::i2s_data_in, fnk0104b::pins::audio::i2c_sda,
        fnk0104b::pins::audio::i2c_scl, fnk0104b::pins::audio::codec_i2c_address});
    if (micReady) {
      voicePreparing = false;
      recording.store(true);
      Serial.println("monitor_voice state=recording");
      uiDirty = true;
    }
    if (peripheralMutex) xSemaphoreGive(peripheralMutex);
    if (!micReady) {
      voicePreparing = false;
      Serial.println("monitor_voice state=error reason=microphone_unavailable");
      recording = false; voiceBusy = false; uiDirty = true;
      setMessage("Microphone unavailable"); continue;
    }
    setMessage("Listening: tap to stop", 12000);
    pcm = static_cast<int16_t*>(ps_malloc(kRecordSamples * sizeof(int16_t)));
    if (!pcm) pcm = static_cast<int16_t*>(malloc(kRecordSamples * sizeof(int16_t)));
    size_t captured = 0;
    bool ok = pcm && fnk0104b::microphone.capture(pcm, kRecordSamples, captured, kRecordSeconds * 1000 + 600,
                                                   captureStop, nullptr);
    if (peripheralMutex) xSemaphoreTake(peripheralMutex, portMAX_DELAY);
    fnk0104b::microphone.end();
    if (peripheralMutex) xSemaphoreGive(peripheralMutex);
    recording = false;
    Serial.printf("monitor_voice state=captured samples=%u duration_ms=%u\n",
                  static_cast<unsigned>(captured), static_cast<unsigned>(captured * 1000 / kSampleRate));
    uiDirty = true;
    if (!ok || captured == 0) {
      free(pcm); pcm = nullptr; voiceBusy = false; uiDirty = true;
      setMessage("Recording failed"); continue;
    }
    const size_t pcmBytes = captured * sizeof(int16_t);
    wav = static_cast<uint8_t*>(ps_malloc(pcmBytes + 44));
    if (!wav) wav = static_cast<uint8_t*>(malloc(pcmBytes + 44));
    if (!wav || !locallink::writeWavHeader(wav, pcmBytes, kSampleRate)) {
      free(pcm); free(wav); pcm = nullptr; wav = nullptr; voiceBusy = false;
      uiDirty = true; setMessage("Audio buffer unavailable"); continue;
    }
    memcpy(wav + 44, pcm, pcmBytes); free(pcm); pcm = nullptr;
    HTTPClient http; WiFiClient client;
    setMessage("Transcribing voice...", 35000);
    String url = String("http://") + MONITOR_SERVER_HOST + ":" + String(MONITOR_SERVER_PORT) + "/v1/voice";
    if (voiceAgent[0]) url += String("?agent_id=") + encodeQueryValue(voiceAgent);
    http.setTimeout(30000);
    if (http.begin(client, url)) {
      http.addHeader("Content-Type", "audio/wav");
      if (MONITOR_SERVER_TOKEN[0]) http.addHeader("X-Monitor-Key", MONITOR_SERVER_TOKEN);
      const int code = http.POST(wav, pcmBytes + 44);
      Serial.printf("monitor_voice state=response http=%d\n", code);
      bool accepted = false;
      bool rejected = false;
      char responseMessage[48] = {};
      const int responseBytes = http.getSize();
      if (responseBytes >= 0 && responseBytes <= 1024) {
        const String response = http.getString();
        DynamicJsonDocument result(1024);
        if (!deserializeJson(result, response)) {
          accepted = code >= 200 && code < 300 && result["ok"].is<bool>() && result["ok"].as<bool>();
          if (accepted) {
            const char* transcript = result["transcript"] | "";
            snprintf(responseMessage, sizeof(responseMessage), "Sent: %.40s", transcript);
          }
          JsonVariantConst error = result["error"];
          rejected = error.is<const char*>() || error.is<JsonObjectConst>();
          if (error.is<JsonObjectConst>()) {
            rejected = error["code"].is<const char*>() || error["message"].is<const char*>();
          }
        }
      }
      setMessage(accepted ? responseMessage : (rejected ? "Bridge rejected voice" : "Voice request failed"), 8000);
      http.end();
    } else setMessage("Bridge unavailable");
    free(wav); wav = nullptr;
    voiceBusy = false;
    uiDirty = true;
  }
}

void beginVoice() {
  if (!voiceTask || !peripheralMutex || WiFi.status() != WL_CONNECTED || !MONITOR_SERVER_HOST[0]) {
    setMessage("Check Wi-Fi and bridge"); return;
  }
  if (voiceBusy.load()) {
    if (recording.load()) stopCapture.store(true);
    else setMessage("Sending voice...");
    return;
  }
  voiceBusy.store(true);
  voicePreparing.store(true);
  stopCapture.store(false);
  setMessage("Preparing microphone", 12000);
  Serial.println("monitor_voice state=preparing");
  uiDirty = true;
  strlcpy(voiceAgent, selectedAgent, sizeof(voiceAgent));
  xTaskNotifyGive(voiceTask);
}

void touchLoop() {
  // Codec setup may hold the shared I2C bus; keep drawing while it does.
  if (peripheralMutex && xSemaphoreTake(peripheralMutex, 0) != pdTRUE) return;
  const bool gotTouch = fnk0104b::touch.read(touchPoint);
  if (peripheralMutex) xSemaphoreGive(peripheralMutex);
  if (!gotTouch) return;
  if (touchPoint.pressed) {
    idleTimer.activity(millis());
    if (!screenAwake) {
      // Do not show cached quotas as current after an unattended interval.
      markOffline();
      screenAwake = true;
      setMessage("Refreshing status", 6000);
      fnk0104b::display.setBacklight(true);
      resumeStatus();
      Serial.println("monitor_sleep state=awake reason=touch");
      drawScreen(); previousTouch = true; return;
    }
    if (previousTouch) return;
    // Shared board helper already reports rotation-1 landscape coordinates.
    const int x = touchPoint.x;
    const int y = touchPoint.y;
    if (y >= 191 && y <= 237 && x >= 4 && x <= 315) beginVoice();
    else if (y >= 59 && y < 184 && selectedAgent[0]) {
      selectedAgent[0] = '\0';
      uiDirty = true;
    } else if (y >= 59 && y < 184 && x >= 8 && x <= 312) {
      MonitorStatus snapshot; portENTER_CRITICAL(&statusMux); snapshot = status; portEXIT_CRITICAL(&statusMux);
      const uint8_t index = snapshot.count == 1 ? 0 :
          static_cast<uint8_t>((y >= 123 ? 2 : 0) + (x >= 164 ? 1 : 0));
      if (index < snapshot.count && !strcmp(snapshot.agents[index].status, "needs_attention")) {
        strlcpy(selectedAgent, snapshot.agents[index].id, sizeof(selectedAgent));
        selectedDetail = snapshot.agents[index];
        uiDirty = true;
      }
      else if (selectedAgent[0]) {
        selectedAgent[0] = '\0';
        uiDirty = true;
      }
    }
  }
  previousTouch = touchPoint.pressed;
}

}  // namespace

void setup() {
  fnk0104b::board.begin();
  fnk0104b::board.printStartupInfo("codex-monitor", "0.4.0");
  fnk0104b::display.begin(1);
  fnk0104b::touch.begin();
  peripheralMutex = xSemaphoreCreateMutex();
  streamMutex = xSemaphoreCreateMutex();
  if (!peripheralMutex) setMessage("I/O unavailable");
  fnk0104b::touch.read(touchPoint);
  preferences.begin("monitor", false);
  volumePercent = preferences.getUChar("volume", 50);
  if (volumePercent > 100) volumePercent = 50;
  screenTimeoutMinutes = preferences.getUShort("timeout", kScreenTimeoutDefault);
  if (screenTimeoutMinutes < 1 || screenTimeoutMinutes > 120) screenTimeoutMinutes = kScreenTimeoutDefault;
  idleTimer.activity(millis());
  WiFi.mode(WIFI_STA); WiFi.setAutoReconnect(true); WiFi.begin();
  startBle();
  if (xTaskCreatePinnedToCore(voiceWorker, "monitor-voice", 8192, nullptr, 1, &voiceTask, 1) != pdPASS) {
    voiceTask = nullptr;
  }
  if (!streamMutex || xTaskCreatePinnedToCore(statusWorker, "monitor-status", 12288, nullptr, 1, &statusTask, 0) != pdPASS) {
    statusTask = nullptr;
    setMessage("Status stream unavailable", 10000);
  }
  setMessage(MONITOR_SERVER_HOST[0] ? "Connecting to bridge" : "Set monitor server host", 6000);
  drawScreen();
}

void loop() {
  touchLoop();
  refreshSelectedAgent();
  playPendingAttentionTone();
  const bool wifiConnected = WiFi.status() == WL_CONNECTED;
  if (wifiConnected != previousWifiConnected) {
    previousWifiConnected = wifiConnected;
    Serial.printf("monitor_wifi connected=%s status=%d\n",
                  wifiConnected ? "true" : "false", static_cast<int>(WiFi.status()));
    uiDirty = true;
  }
  uint32_t expiry;
  portENTER_CRITICAL(&messageMux);
  expiry = messageUntil;
  portEXIT_CRITICAL(&messageMux);
  const bool messageVisible = static_cast<int32_t>(expiry - millis()) > 0;
  if (messageVisible != previousMessageVisible) {
    previousMessageVisible = messageVisible;
    uiDirty = true;
  }
  uint16_t activeAgents;
  portENTER_CRITICAL(&statusMux);
  activeAgents = status.total_agents;
  portEXIT_CRITICAL(&statusMux);
  // Pending input/error agents and voice work also keep the monitor visible.
  const bool busy = activeAgents > 0 || recording.load() || voicePreparing.load() || voiceBusy.load();
  const bool idleExpired = idleTimer.expired(millis(), static_cast<uint32_t>(screenTimeoutMinutes) * 60000UL, busy);
  if (busy && !screenAwake) {
    screenAwake = true;
    fnk0104b::display.setBacklight(true);
    resumeStatus();
    Serial.println("monitor_sleep state=awake reason=agent_or_voice");
    uiDirty = true;
  } else if (screenAwake && idleExpired) {
    screenAwake = false;
    if (streamMutex) pauseStatus();
    fnk0104b::display.setBacklight(false);
    Serial.println("monitor_sleep state=quiet stream=closed");
  }
  if (screenAwake && uiDirty.exchange(false)) {
    drawScreen();
    lastAvatarFrame = millis();
  } else if (screenAwake && millis() - lastAvatarFrame >= kAvatarFrameMs) {
    drawAnimatedAvatars();
    lastAvatarFrame = millis();
  }
  delay(10);
}
