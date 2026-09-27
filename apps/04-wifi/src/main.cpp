#include <Arduino.h>
#include <WiFi.h>
#include <WiFiProv.h>
#include <esp_system.h>
#include <qrcode.h>

#include <fnk0104b/board.hpp>

namespace {

constexpr char kServiceName[] = "PROV_FN0104B";
char proof_of_possession[13];
uint32_t next_status_at = 0;

void drawProvisioningQr(esp_qrcode_handle_t qr) {
  TFT_eSPI& tft = fnk0104b::display.driver();
  const int modules = esp_qrcode_get_size(qr);
  constexpr int kQuietZoneModules = 4;
  constexpr int kHorizontalPadding = 20;
  constexpr int kFooterHeight = 42;

  const int qrModules = modules + 2 * kQuietZoneModules;
  const int modulePixels = min((tft.width() - kHorizontalPadding) / qrModules,
                               (tft.height() - kFooterHeight) / qrModules);
  if (modulePixels < 1) {
    Serial.println("wifi_qr=display_too_small");
    return;
  }

  const int qrPixels = qrModules * modulePixels;
  const int left = (tft.width() - qrPixels) / 2;
  const int top = (tft.height() - kFooterHeight - qrPixels) / 2;

  tft.fillScreen(TFT_WHITE);
  for (int y = 0; y < modules; ++y) {
    for (int x = 0; x < modules; ++x) {
      if (esp_qrcode_get_module(qr, x, y)) {
        tft.fillRect(left + (x + kQuietZoneModules) * modulePixels,
                     top + (y + kQuietZoneModules) * modulePixels,
                     modulePixels, modulePixels, TFT_BLACK);
      }
    }
  }

  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_BLACK, TFT_WHITE);
  tft.drawString("Scan with ESP SoftAP app", tft.width() / 2,
                 tft.height() - 27, 2);
  tft.drawString(kServiceName, tft.width() / 2, tft.height() - 10, 1);
  Serial.printf("wifi_qr=displayed qr_modules=%d module_pixels=%d\n",
                modules, modulePixels);
}

void onWiFiEvent(arduino_event_t* event) {
  switch (event->event_id) {
    case ARDUINO_EVENT_PROV_START:
      Serial.println("provisioning=started transport=softap");
      break;
    case ARDUINO_EVENT_PROV_CRED_SUCCESS:
      Serial.println("provisioning=credentials_saved");
      break;
    case ARDUINO_EVENT_PROV_CRED_FAIL:
      Serial.println("provisioning=failed; check network name and password");
      break;
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      Serial.print("wifi_ip=");
      Serial.println(WiFi.localIP());
      break;
    default:
      break;
  }
}

}  // namespace

void setup() {
  fnk0104b::board.begin();
  fnk0104b::board.printStartupInfo("wifi-provisioning", "0.2.0");
  fnk0104b::display.begin(1);

  // Create a fresh proof of possession for this provisioning session. The
  // QR code carries it to the phone; it is not a Wi-Fi network password.
  const uint32_t randomA = esp_random();
  const uint32_t randomB = esp_random();
  snprintf(proof_of_possession, sizeof(proof_of_possession), "%06lX%06lX",
           static_cast<unsigned long>(randomA & 0xFFFFFF),
           static_cast<unsigned long>(randomB & 0xFFFFFF));

  WiFi.onEvent(onWiFiEvent);

  WiFiProv.beginProvision(WIFI_PROV_SCHEME_SOFTAP,
                          WIFI_PROV_SCHEME_HANDLER_NONE,
                          WIFI_PROV_SECURITY_1,
                          proof_of_possession,
                          kServiceName,
                          nullptr,
                          nullptr,
                          false);

  char payload[150];
  snprintf(payload, sizeof(payload),
           "{\"ver\":\"v1\",\"name\":\"%s\",\"pop\":\"%s\","
           "\"transport\":\"softap\"}",
           kServiceName, proof_of_possession);
  esp_qrcode_config_t qrConfig = ESP_QRCODE_CONFIG_DEFAULT();
  qrConfig.display_func = drawProvisioningQr;
  const esp_err_t qrResult = esp_qrcode_generate(&qrConfig, payload);
  Serial.printf("wifi_qr_result=%s service=%s\n",
                qrResult == ESP_OK ? "ok" : "error", kServiceName);
}

void loop() {
  if (static_cast<int32_t>(millis() - next_status_at) >= 0) {
    Serial.printf("wifi_status=%s\n",
                  WiFi.status() == WL_CONNECTED ? "connected" : "awaiting_provisioning");
    next_status_at = millis() + 10000;
  }
  delay(250);
}
