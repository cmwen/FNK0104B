#include "monitor_wifi_setup.hpp"
#include "monitor_configuration.hpp"
#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <bootloader_random.h>
#include <esp_random.h>
#include <esp_wifi.h>
#include <network_provisioning/manager.h>
#include <network_provisioning/scheme_ble.h>
#include <fnk0104b/board.hpp>
#include <atomic>

namespace monitor_wifi_setup {
namespace {
char proof[13]{};
uint8_t random[6]{};
bool prepared = false;
bool explicitlyRequested = false;
wifi_config_t previousConfig{};
enum class Stage { Ready, Failed, Connected, Error };
std::atomic<Stage> stage{Stage::Ready};
Stage drawn = Stage::Error;
uint32_t started = 0;
std::atomic<bool> ended{false};
bool hadTouch = true;
bool managerInitialized = false;
std::atomic<bool> bridgeSaved{false};
uint32_t bridgeSavedAt = 0;

esp_err_t bridgeEndpoint(uint32_t, const uint8_t* input, ssize_t length,
                         uint8_t** output, ssize_t* outputLength, void*) {
  if (!input || length <= 0 || length > 480) return ESP_ERR_INVALID_ARG;
  // Provisioning runs on a Bluetooth task: keep JSON buffers off its stack.
  DynamicJsonDocument request(768);
  DynamicJsonDocument response(384);
  Preferences storage;
  const bool opened = storage.begin("monitor", false);
  monitor_config::Bridge current;
  if (opened) monitor_config::readBridge(storage, current);
  const char* error = nullptr;
  bool changed = false;
  if (deserializeJson(request, input, length) || !request.is<JsonObject>()) error = "invalid_request";
  else if (!opened) error = "storage_unavailable";
  else if (!strcmp(request["op"] | "", "read")) {}
  else {
    monitor_config::Bridge next;
    if (!monitor_config::bridgeRequest(request.as<JsonVariantConst>(), current, next)) error = "invalid_bridge";
    else if (!monitor_config::saveBridge(storage, next)) error = "storage_failed";
    else { current = next; changed = true; }
  }
  if (opened) storage.end();
  response["ok"] = error == nullptr;
  if (error) response["error"] = error;
  // Never send the token back, including on errors or readback.
  response["host"] = current.host; response["port"] = current.port;
  response["has_token"] = current.token[0] != 0;
  if (response.overflowed()) return ESP_ERR_NO_MEM;
  *outputLength = measureJson(response);
  *output = static_cast<uint8_t*>(malloc(*outputLength + 1));
  if (!*output) return ESP_ERR_NO_MEM;
  serializeJson(response, reinterpret_cast<char*>(*output), *outputLength + 1);
  if (changed) bridgeSaved = true; // UI task owns restart timing and drawing.
  return ESP_OK;
}
void event(void*, network_prov_cb_event_t id, void*) {
  if (id == NETWORK_PROV_WIFI_CRED_FAIL) stage = Stage::Failed;
  if (id == NETWORK_PROV_WIFI_CRED_SUCCESS) stage = Stage::Connected;
  if (id == NETWORK_PROV_END) ended = true;
}
void restart(bool cancel) {
  if (cancel) {
    if (managerInitialized) network_prov_mgr_deinit();
    // Entry only clears RAM. Restore saved credentials if a failed attempt changed them.
    esp_wifi_disconnect();
    esp_wifi_set_storage(WIFI_STORAGE_FLASH);
    esp_wifi_set_config(WIFI_IF_STA, &previousConfig);
    Preferences prefs;
    prefs.begin("monitor", false);
    prefs.putUChar("wifiSetup", 2);  // Skip setup on the next boot, even without credentials.
    prefs.end();
  }
  ESP.restart();
}
void draw() {
  auto& d = fnk0104b::display.driver();
  d.fillScreen(TFT_BLACK);
  d.setTextDatum(MC_DATUM);
  d.setTextColor(TFT_WHITE, TFT_BLACK);
  d.drawString("Monitor connection setup", 160, 25, 2);
  d.drawString("FNK0104B-SETUP", 160, 60, 2);
  d.setTextColor(TFT_YELLOW, TFT_BLACK);
  d.drawString(proof, 160, 100, 4);
  d.setTextColor(TFT_WHITE, TFT_BLACK);
  const Stage current = stage.load();
  d.drawString(bridgeSaved ? "Bridge saved - restarting shortly" : current == Stage::Connected ? "Connected - restarting shortly" :
               current == Stage::Failed ? "Wi-Fi failed: cancel and retry" :
               current == Stage::Error ? "Setup unavailable: cancel to exit" :
               "Enter code on Web BLE page", 160, 142, 2);
  d.drawString("Configure Wi-Fi or monitor bridge", 160, 172, 2);
  d.drawRect(35, 195, 250, 35, TFT_WHITE);
  d.drawString("Cancel / use monitor offline", 160, 212, 2);
  drawn = current;
}
}
void prepare() {
  if (prepared) return;
  // The bootloader entropy source must be disabled before Wi-Fi/audio starts.
  bootloader_random_enable();
  esp_fill_random(random, sizeof(random));
  bootloader_random_disable();
  prepared = true;
}
bool begin(bool requested, bool skip) {
  prepare();
  explicitlyRequested = requested;
  WiFi.STA.begin(false);
  if (esp_wifi_get_config(WIFI_IF_STA, &previousConfig) != ESP_OK) {
    // Do not infer that credentials are absent on an initialization failure.
    return false;
  }
  if (skip || (!requested && previousConfig.sta.ssid[0])) return false;
  snprintf(proof, sizeof(proof), "%02X%02X%02X%02X%02X%02X",
           random[0], random[1], random[2], random[3], random[4], random[5]);
  network_prov_mgr_config_t config{};
  config.scheme = network_prov_scheme_ble;
  config.scheme_event_handler = NETWORK_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BTDM;
  config.app_event_handler = {event, nullptr};
  esp_err_t result = network_prov_mgr_init(config);
  if (result == ESP_OK) {
    managerInitialized = true;
    // Match the proven wifi-ble diagnostic and browser client UUID.
    uint8_t uuid[] = {0xb4,0xdf,0x5a,0x1c,0x3f,0x6b,0xf4,0xbf,
                      0xea,0x4a,0x82,0x03,0x04,0x90,0x1a,0x02};
    result = network_prov_scheme_ble_set_service_uuid(uuid);
    if (result == ESP_OK) result = network_prov_mgr_endpoint_create("monitor-bridge");
    if (result == ESP_OK)
      result = network_prov_mgr_start_provisioning(NETWORK_PROV_SECURITY_1, proof,
                                                  "FNK0104B-SETUP", nullptr);
    if (result == ESP_OK)
      result = network_prov_mgr_endpoint_register("monitor-bridge", bridgeEndpoint, nullptr);
  }
  if (result != ESP_OK) stage = Stage::Error;
  Serial.printf("monitor_wifi_setup security=1 result=%d\n", result);
  started = millis();
  draw();
  return true;
}
void loop() {
  if (bridgeSaved && !bridgeSavedAt) { bridgeSavedAt = millis(); draw(); }
  if (bridgeSavedAt && millis() - bridgeSavedAt >= 3000) restart(stage != Stage::Connected);
  if (stage.load() != drawn) draw();
  // Manager auto-stop leaves time for the browser to query connected status.
  if (ended && stage == Stage::Connected) restart(false);
  if (millis() - started >= 300000 && stage != Stage::Connected) restart(true);
  fnk0104b::TouchPoint point{};
  if (fnk0104b::touch.read(point)) {
    if (point.pressed && !hadTouch && point.y >= 195 && point.y <= 230 &&
        point.x >= 35 && point.x <= 285) restart(stage != Stage::Connected);
    hadTouch = point.pressed;
  }
  delay(20);
}
void yieldToMicro() {
  if (!explicitlyRequested) {
    Serial.println("monitor_wifi_setup exit=usb_micro restart=skip_setup_once");
    restart(true);
  }
}
void request() {
  Preferences prefs;
  prefs.begin("monitor", false);
  if (prefs.putUChar("wifiSetup", 1) == 1) {
    prefs.end();
    ESP.restart();
  }
  prefs.end();
}
}
