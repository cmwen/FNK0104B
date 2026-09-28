#include <Arduino.h>
#include <WiFi.h>
#include <WiFiProv.h>
#include <bootloader_random.h>
#include <esp_random.h>
#include <atomic>

#include <fnk0104b/board.hpp>

namespace {

constexpr char kServiceName[] = "FNK0104B-SETUP";
char proof_of_possession[13] = {};

enum class Stage : uint8_t {
  kStarting,
  kReady,
  kFailed,
  kConnected,
};

std::atomic<Stage> stage{Stage::kStarting};
Stage last_drawn = Stage::kStarting;
uint32_t next_status_at = 0;

void drawStatus(Stage current) {
  TFT_eSPI& tft = fnk0104b::display.driver();
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("Wi-Fi setup over BLE", tft.width() / 2, 28, 2);

  if (current == Stage::kConnected) {
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.drawString("Wi-Fi connected", tft.width() / 2, 91, 4);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("Install speech firmware next", tft.width() / 2, 155, 2);
  } else {
    tft.drawString(kServiceName, tft.width() / 2, 71, 2);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.drawString(proof_of_possession, tft.width() / 2, 116, 4);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(current == Stage::kFailed ? "Wi-Fi connection failed"
                                              : "Enter code on flasher page",
                   tft.width() / 2, 164, 2);
    tft.drawString("Use a 2.4 GHz network", tft.width() / 2, 195, 2);
  }
}

void onWiFiEvent(arduino_event_t* event) {
  switch (event->event_id) {
    case ARDUINO_EVENT_PROV_START:
      stage.store(Stage::kReady);
      Serial.println("ble_provisioning=ready");
      break;
    case ARDUINO_EVENT_PROV_CRED_SUCCESS:
      Serial.println("ble_provisioning=credentials_accepted");
      break;
    case ARDUINO_EVENT_PROV_CRED_FAIL:
      stage.store(Stage::kFailed);
      Serial.println("ble_provisioning=connection_failed");
      break;
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      stage.store(Stage::kConnected);
      Serial.println("wifi_status=connected");
      break;
    default:
      break;
  }
}

}  // namespace

void setup() {
  fnk0104b::board.begin();
  fnk0104b::board.printStartupInfo("wifi-ble-setup", "0.1.0");
  fnk0104b::display.begin(1);

  uint8_t random_bytes[6] = {};
  bootloader_random_enable();
  esp_fill_random(random_bytes, sizeof(random_bytes));
  bootloader_random_disable();
  snprintf(proof_of_possession, sizeof(proof_of_possession),
           "%02X%02X%02X%02X%02X%02X", random_bytes[0], random_bytes[1],
           random_bytes[2], random_bytes[3], random_bytes[4], random_bytes[5]);
  drawStatus(Stage::kStarting);

  WiFi.onEvent(onWiFiEvent);
  WiFiProv.beginProvision(WIFI_PROV_SCHEME_BLE,
                          WIFI_PROV_SCHEME_HANDLER_FREE_BTDM,
                          WIFI_PROV_SECURITY_1, proof_of_possession,
                          kServiceName, nullptr, nullptr, false);
  Serial.printf("ble_device=%s\n", kServiceName);
  Serial.println("ble_security=security1_pop_on_display");
}

void loop() {
  const Stage current = stage.load();
  if (current != last_drawn) {
    drawStatus(current);
    last_drawn = current;
  }
  if (static_cast<int32_t>(millis() - next_status_at) >= 0) {
    Serial.printf("wifi_status=%s\n",
                  WiFi.status() == WL_CONNECTED ? "connected" : "waiting");
    next_status_at = millis() + 10000;
  }
  delay(100);
}
