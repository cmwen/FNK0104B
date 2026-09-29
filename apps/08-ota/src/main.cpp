#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include <esp_ota_ops.h>
#include <time.h>

#include <fnk0104b/board.hpp>

namespace {

constexpr char kFirmwareVersion[] = "0.3.0";
constexpr char kReleaseApiUrl[] =
    "https://api.github.com/repos/cmwen/FNK0104B/releases/latest";
constexpr time_t kValidClockStart = 1700000000;
constexpr uint32_t kTouchPollMs = 25;

// Use the framework's compact Mozilla root CA bundle for GitHub TLS.
extern const uint8_t kRootCertificateBundle[]
    asm("_binary_x509_crt_bundle_start");

struct Button {
  int16_t x;
  int16_t y;
  int16_t width;
  int16_t height;
};

String availableVersion;
String availableFirmwareUrl;
String screenMessage = "Starting";
bool updateAvailable = false;
bool checkRequested = false;
bool checking = false;
bool installing = false;
bool touchReady = false;
int lastReportedPercent = -5;
uint32_t lastTouchPollAt = 0;
uint32_t lastTouchErrorAt = 0;
bool previousTouch = false;
fnk0104b::TouchPoint touchPoint{0, 0, false};

TFT_eSPI& tft() { return fnk0104b::display.driver(); }

Button checkButton() {
  const int16_t width = tft().width() - 40;
  return {20, static_cast<int16_t>(tft().height() - 58), width, 42};
}

Button installButton() {
  const int16_t width = (tft().width() - 54) / 2;
  return {18, static_cast<int16_t>(tft().height() - 58), width, 42};
}

Button skipButton() {
  const int16_t width = (tft().width() - 54) / 2;
  return {static_cast<int16_t>(36 + width),
          static_cast<int16_t>(tft().height() - 58), width, 42};
}

bool contains(const Button& button, int16_t x, int16_t y) {
  return x >= button.x && x < button.x + button.width && y >= button.y &&
         y < button.y + button.height;
}

void drawButton(const Button& button, const char* label, uint16_t color) {
  tft().fillRoundRect(button.x, button.y, button.width, button.height, 7,
                      color);
  tft().drawRoundRect(button.x, button.y, button.width, button.height, 7,
                      TFT_WHITE);
  tft().setTextDatum(MC_DATUM);
  tft().setTextColor(TFT_WHITE, color);
  tft().drawString(label, button.x + button.width / 2,
                   button.y + button.height / 2, 2);
}

void drawScreen() {
  tft().fillScreen(TFT_BLACK);
  tft().setTextDatum(TL_DATUM);
  tft().setTextColor(TFT_CYAN, TFT_BLACK);
  tft().drawString("FNK0104B OTA", 16, 12, 4);

  tft().setTextColor(TFT_WHITE, TFT_BLACK);
  tft().drawString(String("Wi-Fi: ") +
                       (WiFi.status() == WL_CONNECTED ? "connected" : "waiting"),
                   18, 50, 2);
  tft().drawString(String("Installed: ") + kFirmwareVersion, 18, 78, 2);
  if (!availableVersion.isEmpty()) {
    tft().drawString(String("Latest: ") + availableVersion, 18, 102, 2);
  }

  tft().setTextColor(TFT_YELLOW, TFT_BLACK);
  tft().drawString(screenMessage, 18, 135, 2);

  if (installing) {
    tft().drawRect(18, 174, tft().width() - 36, 15, TFT_WHITE);
    return;
  }
  if (checking) return;

  if (updateAvailable) {
    drawButton(installButton(), "INSTALL", TFT_DARKGREEN);
    drawButton(skipButton(), "LATER", TFT_DARKGREY);
  } else {
    drawButton(checkButton(), "CHECK AGAIN", TFT_DARKCYAN);
  }
}

String normalizedVersion(const String& version) {
  return version.startsWith("v") ? version.substring(1) : version;
}

bool syncClock() {
  Serial.println("ota_clock=syncing");
  configTime(0, 0, "pool.ntp.org", "time.cloudflare.com");
  const uint32_t startedAt = millis();
  while (time(nullptr) < kValidClockStart && millis() - startedAt < 30000) {
    delay(250);
  }
  const bool synchronized = time(nullptr) >= kValidClockStart;
  Serial.printf("ota_clock=%s\n", synchronized ? "ready" : "failed");
  return synchronized;
}

bool fetchLatestRelease(String& version, String& firmwareUrl) {
  WiFiClientSecure client;
  client.setCACertBundle(kRootCertificateBundle);
  client.setTimeout(30);

  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  if (!http.begin(client, kReleaseApiUrl)) {
    Serial.println("ota_check=failed reason=http_begin");
    return false;
  }
  http.addHeader("User-Agent", "FNK0104B-OTA-Demo");
  http.addHeader("Accept", "application/vnd.github+json");
  const int status = http.GET();
  if (status != HTTP_CODE_OK) {
    Serial.printf("ota_check=failed http_status=%d\n", status);
    http.end();
    return false;
  }

  StaticJsonDocument<256> filter;
  filter["tag_name"] = true;
  filter["assets"][0]["name"] = true;
  filter["assets"][0]["browser_download_url"] = true;
  StaticJsonDocument<512> release;
  const DeserializationError error = deserializeJson(
      release, http.getStream(), DeserializationOption::Filter(filter));
  http.end();
  if (error) {
    Serial.printf("ota_check=failed reason=json error=%s\n", error.c_str());
    return false;
  }

  version = release["tag_name"] | "";
  if (version.isEmpty()) {
    Serial.println("ota_check=failed reason=release_tag_missing");
    return false;
  }
  for (JsonObject asset : release["assets"].as<JsonArray>()) {
    if (String(asset["name"] | "") == "ota.bin") {
      firmwareUrl = asset["browser_download_url"] | "";
      break;
    }
  }
  if (firmwareUrl.isEmpty()) {
    Serial.printf("ota_check=failed reason=asset_missing name=ota.bin tag=%s\n",
                  version.c_str());
    return false;
  }
  return true;
}

void checkForUpdate() {
  checking = true;
  updateAvailable = false;
  screenMessage = "Checking GitHub";
  drawScreen();

  if (WiFi.status() != WL_CONNECTED) {
    screenMessage = "Waiting for Wi-Fi";
    checking = false;
    Serial.println("ota_check=waiting_for_wifi");
    drawScreen();
    return;
  }
  if (!syncClock()) {
    screenMessage = "Clock unavailable; see serial";
    checking = false;
    Serial.println("ota_check=failed reason=tls_clock_unavailable");
    drawScreen();
    return;
  }

  String releaseVersion;
  String firmwareUrl;
  if (!fetchLatestRelease(releaseVersion, firmwareUrl)) {
    screenMessage = "Check failed; see serial";
    checking = false;
    drawScreen();
    return;
  }

  availableVersion = releaseVersion;
  availableFirmwareUrl = firmwareUrl;
  updateAvailable =
      normalizedVersion(availableVersion) != String(kFirmwareVersion);
  checking = false;
  Serial.printf("ota_current_version=%s\n", kFirmwareVersion);
  Serial.printf("ota_latest_version=%s\n", availableVersion.c_str());
  if (updateAvailable) {
    screenMessage = "New version available";
    Serial.println("ota_update_available=yes");
    Serial.println("ota_prompt=press_install_or_later");
  } else {
    screenMessage = "Already up to date";
    Serial.println("ota_status=up_to_date");
  }
  drawScreen();
}

void printUpdateProgress(int current, int total) {
  if (total <= 0) return;
  const int percent = static_cast<int>((100LL * current) / total);
  if (percent >= lastReportedPercent + 5 || percent == 100) {
    Serial.printf("ota_progress=%d%% bytes=%d/%d\n", percent, current, total);
    lastReportedPercent = percent;
    const int16_t barWidth = static_cast<int16_t>(tft().width() - 40);
    const int16_t filled = static_cast<int16_t>((barWidth * percent) / 100);
    tft().fillRect(20, 176, filled, 11, TFT_GREEN);
    tft().fillRect(20 + filled, 176, barWidth - filled, 11, TFT_BLACK);
  }
}

void installAvailableUpdate() {
  if (!updateAvailable || availableFirmwareUrl.isEmpty()) {
    Serial.println("ota_result=failed reason=no_update_selected");
    return;
  }
  installing = true;
  screenMessage = String("Installing ") + availableVersion;
  drawScreen();

  WiFiClientSecure client;
  client.setCACertBundle(kRootCertificateBundle);
  client.setTimeout(30);

  httpUpdate.rebootOnUpdate(true);
  httpUpdate.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  httpUpdate.onStart([]() {
    lastReportedPercent = -5;
    Serial.println("ota_download=started");
  });
  httpUpdate.onProgress(printUpdateProgress);
  httpUpdate.onEnd([]() { Serial.println("ota_download=complete"); });
  httpUpdate.onError([](int error) {
    Serial.printf("ota_download=error code=%d message=%s\n", error,
                  httpUpdate.getLastErrorString().c_str());
  });

  Serial.printf("ota_installing_version=%s\n", availableVersion.c_str());
  Serial.printf("ota_url=%s\n", availableFirmwareUrl.c_str());
  Serial.println("ota_partition=inactive_slot");
  const t_httpUpdate_return result = httpUpdate.update(
      client, availableFirmwareUrl, kFirmwareVersion);
  if (result == HTTP_UPDATE_OK) {
    Serial.println("ota_result=installed rebooting=1");
  } else if (result == HTTP_UPDATE_NO_UPDATES) {
    installing = false;
    screenMessage = "Already up to date";
    Serial.println("ota_result=no_update");
    drawScreen();
  } else {
    installing = false;
    screenMessage = "Install failed; see serial";
    Serial.printf("ota_result=failed code=%d message=%s\n",
                  httpUpdate.getLastError(),
                  httpUpdate.getLastErrorString().c_str());
    drawScreen();
  }
}

void printInstructions() {
  Serial.println("ota_instructions=touch_check_install_later");
  Serial.println("ota_instructions=serial_c_check_y_install_n_skip");
}

void handleTouch() {
  const uint32_t now = millis();
  if (!touchReady || now - lastTouchPollAt < kTouchPollMs) return;
  lastTouchPollAt = now;

  if (!fnk0104b::touch.read(touchPoint)) {
    if (now - lastTouchErrorAt >= 1000) {
      Serial.println("touch_read=error");
      lastTouchErrorAt = now;
    }
    previousTouch = false;
    return;
  }

  if (touchPoint.pressed && !previousTouch) {
    Serial.printf("touch_x=%d touch_y=%d\n", touchPoint.x, touchPoint.y);
    if (updateAvailable && contains(installButton(), touchPoint.x,
                                    touchPoint.y)) {
      installAvailableUpdate();
    } else if (updateAvailable &&
               contains(skipButton(), touchPoint.x, touchPoint.y)) {
      updateAvailable = false;
      screenMessage = "Update skipped";
      Serial.println("ota_update=skipped");
      drawScreen();
    } else if (!checking && !installing &&
               contains(checkButton(), touchPoint.x, touchPoint.y)) {
      checkRequested = true;
    }
  }
  previousTouch = touchPoint.pressed;
}

}  // namespace

void setup() {
  fnk0104b::board.begin();
  fnk0104b::board.printStartupInfo("ota-demo", kFirmwareVersion);
  const esp_err_t validation = esp_ota_mark_app_valid_cancel_rollback();
  Serial.printf("ota_boot_validation=%s code=%d\n",
                validation == ESP_OK ? "accepted" : "error", validation);

  fnk0104b::display.begin(1);
  touchReady = fnk0104b::touch.begin();
  Serial.printf("touch=%s\n", touchReady ? "ready" : "not_ready");
  screenMessage = "Connecting to Wi-Fi";
  drawScreen();
  printInstructions();

  WiFi.mode(WIFI_STA);
  WiFi.begin();  // Reuse credentials previously saved in device NVS.
  Serial.println("wifi_connect=started using_saved_credentials");
}

void loop() {
  static uint32_t nextStatusAt = 0;
  static bool checkedAfterConnect = false;

  if (WiFi.status() == WL_CONNECTED && !checkedAfterConnect) {
    checkedAfterConnect = true;
    checkRequested = true;
  }

  while (Serial.available() > 0) {
    const char command = static_cast<char>(Serial.read());
    if (command == 'c' || command == 'C') {
      checkRequested = true;
    } else if (command == 'y' || command == 'Y') {
      installAvailableUpdate();
    } else if (command == 'n' || command == 'N') {
      updateAvailable = false;
      screenMessage = "Update skipped";
      Serial.println("ota_update=skipped");
      drawScreen();
    }
  }

  if (millis() >= nextStatusAt) {
    Serial.printf("wifi_status=%s\n",
                  WiFi.status() == WL_CONNECTED ? "connected" : "connecting");
    if (WiFi.status() == WL_CONNECTED) {
      Serial.printf("wifi_ip=%s\n", WiFi.localIP().toString().c_str());
    }
    nextStatusAt = millis() + 10000;
  }

  if (checkRequested) {
    checkRequested = false;
    checkForUpdate();
  }
  handleTouch();
  delay(1);
}
