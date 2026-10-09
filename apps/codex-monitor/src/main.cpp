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
#include <ui/micro_layout.hpp>
#include <codex_hid/voice_controls.hpp>
#include <codex_hid/monitor_settings.hpp>
#include "monitor_speech.hpp"
#include "monitor_commands.hpp"
#include <codex_hid/backend.hpp>
#include <codex_hid/ble_backend.hpp>
#include <fnk0104b/audio_input.hpp>
#include <fnk0104b/usb_microphone.hpp>
#include "voice_capture_gate.hpp"
#include "monitor_wifi_setup.hpp"
#include "monitor_ota.hpp"

#include <atomic>
#include <errno.h>
#include <memory>
#include <new>
#include <esp_timer.h>
#include <esp_random.h>
#include <esp_heap_caps.h>
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
struct AllocationFailure { uint32_t magic, size, caps; };
RTC_NOINIT_ATTR volatile AllocationFailure lastAllocationFailure;
constexpr uint32_t kAllocationFailureMagic = 0x464e4b41;
void IRAM_ATTR allocationFailed(size_t size, uint32_t caps, const char*) {
  // The allocator callback must not allocate or take a logging lock.
  lastAllocationFailure.size = size;
  lastAllocationFailure.caps = caps;
  lastAllocationFailure.magic = kAllocationFailureMagic;
}
constexpr uint32_t kStatusTimeoutMs = 15000;
constexpr uint32_t kStreamSilenceMs = 90000;
constexpr uint32_t kAvatarFrameMs = 100;
constexpr uint32_t kScreenTimeoutDefault = 30;
constexpr uint32_t kSampleRate = 16000;
constexpr uint32_t kRecordSeconds = speech::kVoiceMaxSeconds;
constexpr size_t kRecordSamples = kSampleRate * kRecordSeconds;
constexpr size_t kMaxStatusBytes = 8192;
constexpr char kServiceUuid[] = "4e4b0104-0001-4d20-8f4b-0104b0000001";
constexpr char kCharacteristicUuid[] = "4e4b0104-0002-4d20-8f4b-0104b0000001";
constexpr char kExtendedSettingsUuid[] = "4e4b0104-0003-4d20-8f4b-0104b0000001";
constexpr char kVoiceSettingsUuid[] = "4e4b0104-0004-4d20-8f4b-0104b0000001";
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
  int64_t clock_epoch = -1, clock_received_us = 0;
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
uint32_t lastAvatarFrame = 0, lastResetCheck = 0;
int drawnFiveHourReset = -1, drawnWeeklyReset = -1;
bool screenAwake = true, previousTouch = false;
bool screenManuallyOff = false;
bool showingStatus = false, commandHelpVisible = false;
bool desktopMicHeld = false;
int heldMicroKey = -1;
bool microCommands = false;
int selectedMicroSlot = -1;
codex_hid::Transport previousTransport = codex_hid::Transport::None;
std::atomic<bool> compactMicroLayout{true};
std::atomic<bool> microLayoutChanged{false};
std::atomic<bool> separateVoiceButton{false}, voiceControlsChanged{false};
codex_hid::VoiceControls usbVoiceControls;
std::atomic<bool> voiceOrchestratorOnly{false};
codex_hid::Status desktopStatus{};
std::atomic<bool> usbMicroActive{false};
std::atomic<uint32_t> audioGeneration{0}, requestedAudioGeneration{0};
codex_hid::LinkState previousMicroLink = codex_hid::LinkState::Off;
uint32_t lastVoiceMeterFrame = 0;
int drawnVoiceLevel = -1, drawnMicLevel = -1;
bool previousAudioReady = false, previousUsbStreaming = false;
bool bleConnected = false;
bool wifiSetupMode = false;
uint32_t wifiHoldStarted = 0;
std::atomic<bool> recording{false};
std::atomic<bool> voiceBusy{false};
std::atomic<bool> voicePreparing{false};
std::atomic<bool> uiDirty{true};
std::atomic<bool> statusPaused{false};
std::atomic<bool> refreshOnWake{false};
bool previousMessageVisible = false;
bool previousWifiConnected = false;
bool previousSpeechReady = false;
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
BLECharacteristic* extendedSettingsCharacteristic = nullptr;
BLECharacteristic* voiceSettingsCharacteristic = nullptr;
SemaphoreHandle_t peripheralMutex = nullptr;
SemaphoreHandle_t audioMutex = nullptr;
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
      left.five_hour.resets != right.five_hour.resets ||
      left.weekly.resets != right.weekly.resets ||
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
  if (previousTransport != codex_hid::Transport::None || !snapshot.count || showingStatus || commandHelpVisible || voiceBusy.load()) return;
  if (selectedAgent[0]) drawAvatar(selectedDetail, 18, 74, 96, false);
  else if (snapshot.count == 1) drawAvatar(snapshot.agents[0], 18, 74, 96, false);
  else for (uint8_t i = 0; i < snapshot.count && i < 4; ++i)
    drawAvatar(snapshot.agents[i], 13 + (i % 2) * 156, 65 + (i / 2) * 64, 48, false);
}

int64_t statusTime(const MonitorStatus& snapshot) {
  return snapshot.clock_epoch <= 0 ? -1 : snapshot.clock_epoch +
      (esp_timer_get_time() - snapshot.clock_received_us) / 1000000;
}

void drawStatusBar(const MonitorStatus& snapshot) {
  TFT_eSPI& d = tft();
  const int64_t now = statusTime(snapshot);
  drawnFiveHourReset = ui::monitor::resetPixels(snapshot.five_hour.resets, now, 5 * 3600,
                                               ui::monitor::kFiveHourResetWidth);
  drawnWeeklyReset = ui::monitor::resetPixels(snapshot.weekly.resets, now, 7 * 86400,
                                             ui::monitor::kWeeklyResetWidth);
  bool attention = false, error = false;
  for (uint8_t i = 0; i < snapshot.count; ++i) {
    attention |= !strcmp(snapshot.agents[i].status, "needs_attention");
    error |= !strcmp(snapshot.agents[i].status, "error");
  }
  const bool wifi = WiFi.status() == WL_CONNECTED;
  ui::monitor::statusBar(d, wifi, wifi ? WiFi.RSSI() : -127,
      snapshot.integration, snapshot.count > 0, attention, error,
      ui::monitor::remaining(snapshot.five_hour.used), ui::monitor::remaining(snapshot.weekly.used),
      drawnFiveHourReset, drawnWeeklyReset,
      previousMicroLink == codex_hid::LinkState::Linked ? "Linked" : previousMicroLink == codex_hid::LinkState::Idle ? "Idle" :
      nullptr);
}

void drawVoiceControl() {
  drawnVoiceLevel = monitor_speech::level();
  drawnMicLevel = fnk0104b::audio_input::level();
  if (!codex_hid::microConnected(previousMicroLink)) {
    ui::monitor::voiceControl(tft(), recording.load(), voicePreparing.load(), voiceBusy.load(), selectedAgent[0] != '\0',
        commandHelpVisible, drawnVoiceLevel, monitor_speech::ready(), true);
    return;
  }
  ui::micro::voice(tft(), usbMicroActive, usbMicroActive ? fnk0104b::audio_input::ready() : monitor_speech::ready(),
      desktopMicHeld, fnk0104b::usb_microphone::streaming(), usbMicroActive ? drawnMicLevel : drawnVoiceLevel,
      recording, voicePreparing, voiceBusy, fnk0104b::usb_microphone::muted(),
      usbVoiceControls.voiceEnabled(), usbVoiceControls.voiceOpen());

}

void drawMicroTile(unsigned i) {
  const auto& slot = desktopStatus.slots[i];
  const uint32_t c = slot.color;
  const float b = slot.brightness;
  const uint16_t color = ui::monitor::rgb(uint8_t((c >> 16) * b),
      uint8_t(((c >> 8) & 255) * b), uint8_t((c & 255) * b));
  ui::micro::tile(tft(), i, color, slot.present, selectedMicroSlot == int(i), microCommands);
}

void drawScreen() {
  if (!screenAwake) return;
  TFT_eSPI& d = tft();
  MonitorStatus snapshot;
  portENTER_CRITICAL(&statusMux); snapshot = status; portEXIT_CRITICAL(&statusMux);
  d.fillScreen(kBg);
  drawStatusBar(snapshot);
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
  else if (showingStatus) d.drawString("QUOTA LEFT / TAP CARDS TO RETURN", 10, 45, 1);
  else if (snapshot.count) d.drawString(showingSelected ? "AGENT DETAIL / QUOTA LEFT" : (monitor_speech::ready() ? "HI ESP: COMMANDS / QUOTA LEFT" : "ACTIVE AGENTS / QUOTA LEFT"), 10, 45, 1);
  else d.drawString(!strcmp(snapshot.integration, "connected") ?
      (monitor_speech::ready() ? "HI ESP: COMMANDS / QUOTA LEFT" : "QUOTA LEFT") :
      (monitor_speech::ready() ? "HI ESP: COMMANDS / BRIDGE OFFLINE" : "BRIDGE UNAVAILABLE"), 10, 45, 1);
  if (previousTransport != codex_hid::Transport::None && !commandHelpVisible && !showingStatus && !voiceBusy) {
    if (!messageVisible) {
      d.fillRect(4, 42, 312, 13, kBg);
      d.setTextColor(kMuted, kBg);
      d.drawString(microCommands ? "DESKTOP KEYS / TAP MICRO FOR AGENTS" :
      usbMicroActive ? "USB MICRO / TAP MICRO FOR KEYS" : "BLE MICRO / SPEECH TO ORCHESTRATOR", 10, 45, 1);
    }
    for (unsigned i = 0; i < (microCommands || !compactMicroLayout ? 6u : 3u); ++i)
      drawMicroTile(i);
    if (!microCommands && compactMicroLayout) ui::micro::directions(d);
    drawVoiceControl();
    return;
  }
  if (commandHelpVisible) {
    ui::monitor::commandHelp(d, speech::kMonitorCommands, speech::kMonitorCommandCount);
  } else if (voiceBusy.load()) {
    ui::monitor::messagePanel(d, recording.load(), voicePreparing.load(), voiceAgent[0] != '\0');
  } else if (snapshot.count == 0 || showingStatus) {
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
    d.setTextColor(kMuted, kPanel);
    d.drawString("Tap avatar to message", 128, 162, 1);
  } else {
    for (uint8_t i = 0; i < snapshot.count && i < 4; ++i)
      drawAgentTile(snapshot.agents[i], 8 + (i % 2) * 156, 59 + (i / 2) * 64);
    if (snapshot.total_agents > 4) {
      d.setTextColor(kMuted, kBg);
      d.drawRightString(String("+") + String(snapshot.total_agents - 4) + " more", 310, 45, 1);
    }
  }
  drawVoiceControl();
}

void publishSettings() {
  if (!settingsCharacteristic) return;
  uint8_t bytes[6] = {3, volumePercent, static_cast<uint8_t>(screenTimeoutMinutes & 0xff),
                      static_cast<uint8_t>(screenTimeoutMinutes >> 8),
                      static_cast<uint8_t>(compactMicroLayout ? 3 : 6),
                      static_cast<uint8_t>(separateVoiceButton ? 1 : 0)};
  if (voiceSettingsCharacteristic) voiceSettingsCharacteristic->setValue(bytes, sizeof(bytes));
  bytes[0] = 2;
  if (extendedSettingsCharacteristic) extendedSettingsCharacteristic->setValue(bytes, 5);
  // Keep the published setup page usable before it receives the new selector.
  bytes[0] = 1;
  settingsCharacteristic->setValue(bytes, 4);
}

class SettingsCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* characteristic) override {
    const auto raw = characteristic->getValue();
    const uint8_t version = raw.length() ? static_cast<uint8_t>(raw[0]) : 0;
    codex_hid::MonitorSettings next{volumePercent, screenTimeoutMinutes,
        static_cast<uint8_t>(compactMicroLayout ? 3 : 6), separateVoiceButton};
    if (!codex_hid::decodeMonitorSettings(reinterpret_cast<const uint8_t*>(raw.c_str()), raw.length(), next)) return;
    volumePercent = next.volume; screenTimeoutMinutes = next.timeout;
    preferences.putUChar("volume", volumePercent);
    preferences.putUShort("timeout", screenTimeoutMinutes);
    if (version >= 2) {
      compactMicroLayout = next.slots == 3;
      preferences.putUChar("microSlots", compactMicroLayout ? 3 : 6);
      microLayoutChanged = true;
    }
    if (version == 3) {
      separateVoiceButton = next.separateVoice;
      preferences.putBool("usbVoice", separateVoiceButton);
      voiceControlsChanged = true;
    }
    publishSettings();
  }
};

class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer*, esp_ble_gatts_cb_param_t*) override {
    bleConnected = true;
    Serial.println("monitor_ble connected");
  }
  void onDisconnect(BLEServer*, esp_ble_gatts_cb_param_t*) override {
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
  Serial.println("monitor_ble role=settings hid=disabled");
  BLEService* service = server->createService(BLEUUID(String(kServiceUuid)), 32);
  settingsCharacteristic = service->createCharacteristic(kCharacteristicUuid,
      BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
  settingsCharacteristic->setCallbacks(new SettingsCallbacks());
  settingsCharacteristic->addDescriptor(new BLE2902());
  extendedSettingsCharacteristic = service->createCharacteristic(kExtendedSettingsUuid,
      BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
  extendedSettingsCharacteristic->setCallbacks(new SettingsCallbacks());
  voiceSettingsCharacteristic = service->createCharacteristic(kVoiceSettingsUuid,
      BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
  voiceSettingsCharacteristic->setCallbacks(new SettingsCallbacks());
  monitor_ota::attach(service);
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
  JsonVariantConst updated = doc["updated_at"];
  const int64_t cacheAge = doc["cache_age_seconds"] | int64_t{0};
  if (updated.is<int64_t>() && updated.as<int64_t>() > 0 && cacheAge >= 0 && cacheAge <= 7 * 86400) {
    next.clock_epoch = updated.as<int64_t>() + cacheAge;
    next.clock_received_us = esp_timer_get_time();
  }
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
  if (!pendingAttentionTone || voiceBusy.load() || commandHelpVisible || desktopMicHeld || fnk0104b::usb_microphone::streaming() || !peripheralMutex || !audioMutex) return;
  pendingAttentionTone = false;
  monitor_speech::quietFor(1500);
  xSemaphoreTake(audioMutex, portMAX_DELAY);
  xSemaphoreTake(peripheralMutex, portMAX_DELAY);
  fnk0104b::microphone.end();
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
  xSemaphoreGive(audioMutex);
}

void voiceWorker(void*) {
  int16_t* pcm = nullptr;
  uint8_t* wav = nullptr;
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    if (monitor_ota::runPending()) { uiDirty = true; continue; }
    if (WiFi.status() != WL_CONNECTED || !MONITOR_SERVER_HOST[0]) {
      voicePreparing = false;
      recording = false; voiceBusy = false; uiDirty = true;
      setMessage("Bridge unavailable"); continue;
    }
    const uint32_t captureGeneration = requestedAudioGeneration.load();
    if (usbMicroActive || captureGeneration != audioGeneration.load()) {
      voicePreparing = false; recording = false; voiceBusy = false; uiDirty = true; continue;
    }
    voicePreparing = false;
    recording = true;
    uiDirty = true;
    setMessage("Recording Codex message", (kRecordSeconds + 1) * 1000);
    Serial.println("monitor_voice state=recording source=vadnet");
    pcm = static_cast<int16_t*>(ps_malloc(kRecordSamples * sizeof(int16_t)));
    size_t captured = 0;
    bool ok = pcm && monitor_speech::capture(pcm, kRecordSamples, captured, stopCapture);
    recording = false;
    Serial.printf("monitor_voice state=captured samples=%u duration_ms=%u\n",
                  static_cast<unsigned>(captured), static_cast<unsigned>(captured * 1000 / kSampleRate));
    uiDirty = true;
    if (!ok || captured == 0 || captureGeneration != audioGeneration.load() || usbMicroActive) {
      free(pcm); pcm = nullptr; voiceBusy = false; uiDirty = true;
      setMessage("No speech captured"); continue;
    }
    const size_t pcmBytes = captured * sizeof(int16_t);
    wav = static_cast<uint8_t*>(ps_malloc(pcmBytes + 44));
    if (!wav) wav = static_cast<uint8_t*>(malloc(pcmBytes + 44));
    if (!wav || !locallink::writeWavHeader(wav, pcmBytes, kSampleRate)) {
      free(pcm); free(wav); pcm = nullptr; wav = nullptr; voiceBusy = false;
      uiDirty = true; setMessage("Audio buffer unavailable"); continue;
    }
    memcpy(wav + 44, pcm, pcmBytes); free(pcm); pcm = nullptr;
    if (captureGeneration != audioGeneration.load() || usbMicroActive) {
      free(wav); wav = nullptr; voiceBusy = false; uiDirty = true; continue;
    }
    HTTPClient http; WiFiClient client;
    setMessage("Transcribing voice...", 35000);
    String url = String("http://") + MONITOR_SERVER_HOST + ":" + String(MONITOR_SERVER_PORT) + "/v1/voice";
    if (voiceOrchestratorOnly) url += "?route=orchestrator";
    else if (voiceAgent[0]) url += String("?agent_id=") + encodeQueryValue(voiceAgent);
    http.setTimeout(30000);
    if (http.begin(client, url)) {
      http.addHeader("Content-Type", "audio/wav");
      char requestId[40];
      snprintf(requestId, sizeof(requestId), "%08lx-%08lx-%08lx", static_cast<unsigned long>(esp_random()),
               static_cast<unsigned long>(esp_random()), static_cast<unsigned long>(esp_random()));
      http.addHeader("X-Request-ID", requestId);
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
            const char* action = result["action"] | "";
            if (strcmp(action, "queued") == 0)
              strlcpy(responseMessage, "Voice queued; follow dispatcher avatar", sizeof(responseMessage));
            else
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
  if (usbMicroActive) return;
  if (!voiceTask || !peripheralMutex || !monitor_speech::ready() || WiFi.status() != WL_CONNECTED || !MONITOR_SERVER_HOST[0]) {
    setMessage("Check Wi-Fi and bridge"); return;
  }
  if (voiceBusy.load()) {
    if (recording.load()) stopCapture.store(true);
    else setMessage("Sending voice...");
    return;
  }
  commandHelpVisible = false;
  voiceBusy.store(true);
  voicePreparing.store(true);
  stopCapture.store(false);
  setMessage("Preparing microphone", 12000);
  Serial.println("monitor_voice state=preparing");
  uiDirty = true;
  voiceOrchestratorOnly = previousTransport == codex_hid::Transport::Ble;
  if (voiceOrchestratorOnly) voiceAgent[0] = 0;
  else strlcpy(voiceAgent, selectedAgent, sizeof(voiceAgent));
  requestedAudioGeneration = audioGeneration.load();
  xTaskNotifyGive(voiceTask);
}

void speechLoop() {
  monitor_speech::Event event;
  while (monitor_speech::poll(event)) {
    using Event = monitor_speech::Event;
    if (event == Event::Error) { commandHelpVisible = false; setMessage(monitor_speech::error(), 10000); continue; }
    // A tap takes priority over any local command still queued by recognition.
    if (voiceBusy.load() || usbMicroActive) continue;
    commandHelpVisible = event == Event::Wake;
    uiDirty = true;
    if (event == Event::Timeout) { setMessage("Say Hi ESP to try again"); continue; }
    idleTimer.activity(millis());
    if (event == Event::ScreenOff) {
      if (recording || voiceBusy) { setMessage("Finish voice first"); continue; }
      screenAwake = false;
      screenManuallyOff = true;
      if (streamMutex) pauseStatus();
      fnk0104b::display.setBacklight(false);
      Serial.println("monitor_sleep state=quiet reason=voice");
      continue;
    }
    if (!screenAwake) {
      markOffline();
      screenAwake = true;
      screenManuallyOff = false;
      fnk0104b::display.setBacklight(true);
      resumeStatus();
    }
    switch (event) {
      case Event::Wake: setMessage("Listening for device command", speech::kCommandWindowMs); break;
      case Event::StartListening: beginVoice(); break;
      case Event::GoBack: showingStatus = false; selectedAgent[0] = '\0'; setMessage("Showing agents / overview"); break;
      case Event::ShowStatus:
        showingStatus = true;
        selectedAgent[0] = '\0';
        if (streamMutex) pauseStatus();
        resumeStatus(); setMessage("Refreshing status"); break;
      case Event::ScreenOn: setMessage("Screen on"); break;
      default: break;
    }
    uiDirty = true;
  }
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
      screenManuallyOff = false;
      setMessage("Refreshing status", 6000);
      fnk0104b::display.setBacklight(true);
      resumeStatus();
      Serial.println("monitor_sleep state=awake reason=touch");
      drawScreen(); previousTouch = true; return;
    }
    if (touchPoint.x >= 4 && touchPoint.x <= 80 && touchPoint.y >= 4 && touchPoint.y <= 40) {
      if (!wifiHoldStarted) wifiHoldStarted = millis();
      if (millis() - wifiHoldStarted >= 3000 && !voiceBusy.load()) monitor_wifi_setup::request();
    } else wifiHoldStarted = 0;
    if (previousTouch || heldMicroKey >= 0) return;
    // Shared board helper already reports rotation-1 landscape coordinates.
    const int x = touchPoint.x;
    const int y = touchPoint.y;
    const bool microConnected = previousTransport != codex_hid::Transport::None;
    if (microConnected) {
      int keyId = -1;
      if (x >= 82 && x < 160 && y >= 4 && y <= 40) { microCommands = !microCommands; uiDirty = true; }
      else if (usbMicroActive && ui::micro::bottomAt(x, y) == 11) keyId = 11;
      else if (y >= 191 && y < 237 && x >= 4 && x < (usbMicroActive ? 116 : 204)) {
        if (usbMicroActive) keyId = 10; else beginVoice();
      }
      else if (y >= 191 && y <= 237 && x >= 210 && x <= 315) keyId = 12;
      else if (y >= 59 && y < 184 && showingStatus) { showingStatus = false; uiDirty = true; }
      else {
        const int slot = commandHelpVisible || voiceBusy ? -1 : ui::micro::tileAt(x, y);
        if (!microCommands && compactMicroLayout && !commandHelpVisible && !voiceBusy)
          keyId = ui::micro::directionAt(x, y);
        if (slot >= 0 && (microCommands || !compactMicroLayout || slot < 3)) {
          keyId = microCommands ? ui::micro::commandKeys[slot] : slot;
          if (!usbMicroActive && keyId == 10) { beginVoice(); keyId = -1; }
          if (!microCommands) selectedMicroSlot = slot;
        }
      }
      if (keyId >= 0) {
        if (usbMicroActive && keyId == 10 && !usbVoiceControls.pressPtt()) {
          setMessage("Turn Voice off before hold-to-talk", 5000); previousTouch = true; return;
        }
        if (usbMicroActive && keyId == 11 && !usbVoiceControls.pressVoice(millis())) {
          setMessage("Enable Voice in browser setup / map ACT11", 6000); previousTouch = true; return;
        }
        if (usbMicroActive && (keyId == 10 || keyId == 11))
          fnk0104b::usb_microphone::setMuted(!usbVoiceControls.audioOpen());
        if (codex_hid::key(keyId, true)) {
          heldMicroKey = keyId; desktopMicHeld = keyId == 10;
          if (keyId == 10 || keyId == 11) drawVoiceControl(); else uiDirty = true;
        } else {
          if (keyId == 10 || keyId == 11) {
            usbVoiceControls.reset(); fnk0104b::usb_microphone::setMuted(true);
          }
          setMessage((keyId == 10 || keyId == 11) ? "Desktop HID unavailable / mic off" :
              "Desktop HID unavailable", 6000);
        }
      }
    } else if (y >= 191 && y <= 237 && x >= 4 && x <= 315) beginVoice();
    else if (y >= 59 && y < 184 && (commandHelpVisible || voiceBusy.load())) {
      // Hints and recording replace the agent panel; never select hidden agents.
    } else if (y >= 59 && y < 184 && showingStatus) {
      showingStatus = false;
      uiDirty = true;
    } else if (y >= 59 && y < 184 && selectedAgent[0]) {
      selectedAgent[0] = '\0';
      uiDirty = true;
    } else if (y >= 59 && y < 184 && x >= 8 && x <= 312) {
      MonitorStatus snapshot; portENTER_CRITICAL(&statusMux); snapshot = status; portEXIT_CRITICAL(&statusMux);
      const uint8_t index = snapshot.count == 1 ? 0 :
          static_cast<uint8_t>((y >= 123 ? 2 : 0) + (x >= 164 ? 1 : 0));
      if (index < snapshot.count) {
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
  if (!touchPoint.pressed) {
    wifiHoldStarted = 0;
    // Close the audio gate on physical release even if HID release must retry.
    if (usbMicroActive && heldMicroKey == 10) usbVoiceControls.releasePtt();
    if (usbMicroActive && heldMicroKey == 11) usbVoiceControls.releaseVoice(millis());
    if (usbMicroActive && fnk0104b::usb_microphone::muted() == usbVoiceControls.audioOpen())
      fnk0104b::usb_microphone::setMuted(!usbVoiceControls.audioOpen());
    if (heldMicroKey >= 0 && (codex_hid::key(heldMicroKey, false) || previousTransport == codex_hid::Transport::None)) {
      const bool microphoneRelease = heldMicroKey == 10 || heldMicroKey == 11;
      heldMicroKey = -1; desktopMicHeld = false;
      if (microphoneRelease) drawVoiceControl(); else uiDirty = true;
    }
  }
  previousTouch = touchPoint.pressed;
}

void serialLoop() {
  static char command[24]{};
  static size_t length = 0;
  bool capture = false;
  while (Serial.available()) {
    const char c = static_cast<char>(Serial.read());
    if (c == '\r') continue;
    if (c == '\n') {
      command[length] = '\0';
      capture = strcmp(command, "screenshot") == 0;
      if (!strcmp(command, "ota-check")) monitor_ota::request("check");
      if (!strcmp(command, "ota-install")) monitor_ota::request("install");
      if (!strcmp(command, "hid-agent0"))
        Serial.printf("codex_hid input=%s\n", codex_hid::agent0Tap() ? "queued" : "unavailable");
      if (!strcmp(command, "health"))
        Serial.printf("monitor_health heap=%u psram=%u wifi=%d status_task=%d voice_task=%d speech=%d speech_reason=%s\n",
          unsigned(ESP.getFreeHeap()), unsigned(ESP.getFreePsram()), int(WiFi.status()), statusTask != nullptr,
          voiceTask != nullptr, monitor_speech::ready(), monitor_speech::error());
      length = 0;
      if (capture) break;
    } else if (length < sizeof(command) - 1) command[length++] = c;
    else { length = 0; command[0] = '\0'; }
  }
  if (!capture) return;
  if (voiceBusy.load()) { Serial.println("monitor_screenshot error=voice_busy"); return; }
  // Read the LCD's actual display memory. Freeze UI updates while reading rows;
  // audio and status tasks continue independently. Allocate in PSRAM.
  auto* buffer = static_cast<uint8_t*>(ps_malloc(960 + 2048));
  if (!buffer) { Serial.println("monitor_screenshot error=allocation_failed"); return; }
  char* line = reinterpret_cast<char*>(buffer + 960);
  if (!screenAwake) {
    screenAwake = true; screenManuallyOff = false;
    fnk0104b::display.setBacklight(true);
    resumeStatus();
  }
  idleTimer.activity(millis());
  drawScreen();
  Serial.println("monitor_screenshot begin width=320 height=240 format=rgb888");
  constexpr char hex[] = "0123456789abcdef";
  bool ok = true;
  for (unsigned y = 0; y < 240; ++y) {
    if (!fnk0104b::display.readRowRgb(y, buffer, 960)) { ok = false; break; }
    size_t n = snprintf(line, 2048, "monitor_screenshot row=%u data=", y);
    for (unsigned i = 0; i < 960; ++i) {
      line[n++] = hex[buffer[i] >> 4]; line[n++] = hex[buffer[i] & 15];
    }
    line[n++] = '\n';
    // One write prevents other tasks' status messages splitting a row.
    if (Serial.write(reinterpret_cast<uint8_t*>(line), n) != n) { ok = false; break; }
    delay(1);
  }
  free(buffer);
  Serial.println(ok ? "monitor_screenshot end" : "monitor_screenshot error=read_failed");
}

}  // namespace

void setup() {
  const bool hidStarted = codex_hid::begin();
  fnk0104b::board.begin();
  fnk0104b::board.printStartupInfo("codex-monitor", monitor_ota::version);
  Serial.printf("monitor_startup reset_reason=%u\n", unsigned(esp_reset_reason()));
  if (lastAllocationFailure.magic == kAllocationFailureMagic)
    Serial.printf("monitor_heap previous_failure size=%lu caps=0x%lx\n",
      (unsigned long)lastAllocationFailure.size, (unsigned long)lastAllocationFailure.caps);
  lastAllocationFailure.magic = 0;
  heap_caps_register_failed_alloc_callback(allocationFailed);
  Serial.printf("codex_hid startup=%s wifi=independent\n", hidStarted ? "ready" : "failed");
  fnk0104b::display.begin(1);
  fnk0104b::touch.begin();
  peripheralMutex = xSemaphoreCreateMutex();
  audioMutex = xSemaphoreCreateMutex();
  streamMutex = xSemaphoreCreateMutex();
  if (!peripheralMutex) setMessage("I/O unavailable");
  fnk0104b::touch.read(touchPoint);
  preferences.begin("monitor", false);
  compactMicroLayout = preferences.getUChar("microSlots", 3) != 6;
  separateVoiceButton = preferences.getBool("usbVoice", false);
  usbVoiceControls.configure(separateVoiceButton);
  fnk0104b::usb_microphone::setMuted(true);
  volumePercent = preferences.getUChar("volume", 50);
  if (volumePercent > 100) volumePercent = 50;
  screenTimeoutMinutes = preferences.getUShort("timeout", kScreenTimeoutDefault);
  if (screenTimeoutMinutes < 1 || screenTimeoutMinutes > 120) screenTimeoutMinutes = kScreenTimeoutDefault;
  idleTimer.activity(millis());
  monitor_wifi_setup::prepare();
  fnk0104b::usb_microphone::setAllowed(false);
  const bool usbAudioStarted = fnk0104b::usb_microphone::begin();
  const bool inputStarted = fnk0104b::audio_input::begin(peripheralMutex, audioMutex);
  Serial.printf("monitor_audio startup=%s usb_audio=%s\n", inputStarted ? "ready" : "failed", usbAudioStarted ? "ready" : "failed");
  Serial.printf("monitor_startup stage=wifi_setup heap=%u\n", unsigned(ESP.getFreeHeap()));
  const uint8_t wifiSetupRequest = preferences.getUChar("wifiSetup", 0);
  if (wifiSetupRequest) preferences.remove("wifiSetup");
  wifiSetupMode = monitor_wifi_setup::begin(wifiSetupRequest == 1, wifiSetupRequest == 2);
  if (wifiSetupMode) { monitor_ota::begin(inputStarted && peripheralMutex && audioMutex); return; }
  Serial.printf("monitor_startup stage=wifi heap=%u\n", unsigned(ESP.getFreeHeap()));
  WiFi.mode(WIFI_STA); WiFi.setAutoReconnect(true); WiFi.begin();
  Serial.printf("monitor_startup stage=ble heap=%u\n", unsigned(ESP.getFreeHeap()));
  startBle();
  Serial.printf("monitor_startup stage=workers heap=%u\n", unsigned(ESP.getFreeHeap()));
  if (xTaskCreatePinnedToCore(voiceWorker, "monitor-voice", 8192, nullptr, 1, &voiceTask, 1) != pdPASS) {
    voiceTask = nullptr;
  }
  if (!streamMutex || xTaskCreatePinnedToCore(statusWorker, "monitor-status", 12288, nullptr, 1, &statusTask, 0) != pdPASS) {
    statusTask = nullptr;
    setMessage("Status stream unavailable", 10000);
  }
  setMessage(MONITOR_SERVER_HOST[0] ? "Connecting to bridge" : "Set monitor server host", 6000);
  drawScreen();
  // Keep the established Wi-Fi/BLE startup order and show the UI before the
  // recognition worker starts its model allocations. Raw USB capture is already
  // independent and does not depend on recognition startup.
  Serial.printf("monitor_startup stage=speech heap=%u\n", unsigned(ESP.getFreeHeap()));
  monitor_speech::setEnabled(codex_hid::transport() != codex_hid::Transport::Usb);
  if (!monitor_speech::begin(peripheralMutex, audioMutex, &voiceBusy))
    setMessage("Speech worker unavailable", 10000);
  monitor_ota::begin(inputStarted && peripheralMutex && audioMutex && streamMutex);
  Serial.println("monitor_startup stage=complete");
}

void loop() {
  monitor_ota::tick();
  static bool updateQuiesced = false;
  if (monitor_ota::requested()) {
    const bool allowed = !recording && !voiceBusy && !voicePreparing && !desktopMicHeld && heldMicroKey < 0 && !usbVoiceControls.voiceOpen();
    if (allowed) {
      usbVoiceControls.reset(); fnk0104b::usb_microphone::setMuted(true);
      fnk0104b::usb_microphone::setAllowed(false); monitor_speech::setEnabled(false);
      pauseStatus(); updateQuiesced = true;
    }
    monitor_ota::start(allowed, voiceTask);
  }
  if (monitor_ota::busy()) {
    fnk0104b::usb_microphone::setAllowed(false);
    static uint32_t lastUpdateDraw = 0;
    if (millis() - lastUpdateDraw >= 1000) {
      lastUpdateDraw = millis(); setMessage("Firmware update / microphone off", 2000);
    }
    if (screenAwake && uiDirty) { drawScreen(); uiDirty = false; }
    delay(20); return;
  }
  if (updateQuiesced) {
    updateQuiesced = false;
    monitor_speech::setEnabled(codex_hid::transport() != codex_hid::Transport::Usb);
    if (screenAwake) resumeStatus();
    uiDirty = true;
  }
  if (microLayoutChanged.exchange(false)) uiDirty = true;
  if (voiceControlsChanged.exchange(false)) {
    usbVoiceControls.configure(separateVoiceButton);
    fnk0104b::usb_microphone::setMuted(true);
    uiDirty = true;
  }
  const auto microLink = codex_hid::linkState();
  if (microLink != previousMicroLink) {
    if (!codex_hid::microConnected(microLink)) desktopMicHeld = false;
    previousMicroLink = microLink; uiDirty = true;
  }
  const auto transport = codex_hid::transport();
  const bool usbMode = transport == codex_hid::Transport::Usb;
  if (previousTransport != transport) {
    if (heldMicroKey >= 0) {
      if (previousTransport == codex_hid::Transport::Ble) codex_hid::ble::key(heldMicroKey, false);
      // USB disconnect resets held keys in the host HID connection.
    }
    previousTransport = transport; usbMicroActive = usbMode;
    ++audioGeneration;
    stopCapture = true;
    commandHelpVisible = false; showingStatus = false; selectedAgent[0] = 0;
    selectedMicroSlot = -1; microCommands = false; heldMicroKey = -1; desktopMicHeld = false;
    desktopStatus = codex_hid::Status{};
    usbVoiceControls.reset();
    fnk0104b::usb_microphone::setMuted(true);
    monitor_speech::setEnabled(!usbMode);
    fnk0104b::usb_microphone::setAllowed(false);
    setMessage(usbMode ? "USB Micro connected" : transport == codex_hid::Transport::Ble ? "BLE Micro connected" : "Wi-Fi bridge mode", 4000);
    idleTimer.activity(millis());
    Serial.printf("monitor_mode mode=%s\n", usbMode ? "usb_micro" : transport == codex_hid::Transport::Ble ? "ble_micro" : "bridge");
  }
  fnk0104b::usb_microphone::setAllowed(usbMode && monitor_speech::paused());
  const auto oldDesktopStatus = desktopStatus;
  if (codex_hid::takeStatus(desktopStatus)) {
    bool changed = false;
    for (unsigned i = 0; i < 6; ++i) {
      const auto& before = oldDesktopStatus.slots[i];
      const auto& after = desktopStatus.slots[i];
      const bool tileChanged = !codex_hid::sameAppearance(before, after);
      changed |= tileChanged;
      if (tileChanged && screenAwake && previousTransport != codex_hid::Transport::None &&
          !microCommands && !commandHelpVisible && !showingStatus && !voiceBusy &&
          (!compactMicroLayout || i < 3)) drawMicroTile(i);
    }
    if (changed && previousTransport != codex_hid::Transport::None) {
      idleTimer.activity(millis());
      if (!screenAwake && !screenManuallyOff) {
        screenAwake = true; fnk0104b::display.setBacklight(true); resumeStatus(); uiDirty = true;
      }
    }
    Serial.printf("codex_hid application=status_displayed revision=%lu\n", (unsigned long)desktopStatus.revision);
  }
  const bool audioReady = fnk0104b::audio_input::ready(), usbStreaming = fnk0104b::usb_microphone::streaming();
  if (audioReady != previousAudioReady || usbStreaming != previousUsbStreaming) {
    previousAudioReady = audioReady; previousUsbStreaming = usbStreaming; uiDirty = true;
  }
  if (wifiSetupMode) {
    if (usbMode) monitor_wifi_setup::yieldToMicro();
    monitor_wifi_setup::loop(); return;
  }
  serialLoop();
  speechLoop();
  if (previousSpeechReady != monitor_speech::ready()) {
    previousSpeechReady = monitor_speech::ready();
    uiDirty = true;
  }
  touchLoop();
  if (usbVoiceControls.tick(millis())) {
    fnk0104b::usb_microphone::setMuted(true);
    drawVoiceControl();
  }
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
  const bool busy = desktopMicHeld || usbVoiceControls.voiceOpen() ||
      (fnk0104b::usb_microphone::streaming() && !fnk0104b::usb_microphone::muted()) ||
      monitor_speech::listening() || activeAgents > 0 || recording.load() || voicePreparing.load() || voiceBusy.load();
  const bool idleExpired = idleTimer.expired(millis(), static_cast<uint32_t>(screenTimeoutMinutes) * 60000UL, busy);
  if (busy && !screenAwake && !screenManuallyOff) {
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
  // Refresh only the header when a countdown loses a visible pixel.
  if (screenAwake && millis() - lastResetCheck >= 1000) {
    lastResetCheck = millis();
    MonitorStatus snapshot;
    portENTER_CRITICAL(&statusMux); snapshot = status; portEXIT_CRITICAL(&statusMux);
    const int64_t now = statusTime(snapshot);
    if (drawnFiveHourReset != ui::monitor::resetPixels(snapshot.five_hour.resets, now, 5 * 3600,
                                                     ui::monitor::kFiveHourResetWidth) ||
        drawnWeeklyReset != ui::monitor::resetPixels(snapshot.weekly.resets, now, 7 * 86400,
                                                   ui::monitor::kWeeklyResetWidth))
      drawStatusBar(snapshot);
  }
  // Update only the audio control at 12.5 Hz, leaving the center and header stable.
  if (screenAwake && (desktopMicHeld || fnk0104b::usb_microphone::streaming() || commandHelpVisible || recording.load()) && millis() - lastVoiceMeterFrame >= 80) {
    lastVoiceMeterFrame = millis();
    if (drawnVoiceLevel != monitor_speech::level() || drawnMicLevel != fnk0104b::audio_input::level())
      drawVoiceControl();
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
