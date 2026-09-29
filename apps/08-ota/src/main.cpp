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

constexpr char kFirmwareVersion[] = "0.2.0";
constexpr char kReleaseApiUrl[] =
    "https://api.github.com/repos/cmwen/FNK0104B/releases/latest";
constexpr time_t kValidClockStart = 1700000000;

// Use the framework's compact Mozilla root CA bundle for GitHub TLS.
extern const uint8_t kRootCertificateBundle[]
    asm("_binary_x509_crt_bundle_start");

String availableVersion;
String availableFirmwareUrl;
bool updateAvailable = false;
bool releaseChecked = false;
bool checkRequested = false;
int lastReportedPercent = -5;

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
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("ota_check=waiting_for_wifi");
    return;
  }
  if (!syncClock()) {
    Serial.println("ota_check=failed reason=tls_clock_unavailable");
    return;
  }

  String releaseVersion;
  String firmwareUrl;
  if (!fetchLatestRelease(releaseVersion, firmwareUrl)) return;

  releaseChecked = true;
  availableVersion = releaseVersion;
  availableFirmwareUrl = firmwareUrl;
  updateAvailable =
      normalizedVersion(availableVersion) != String(kFirmwareVersion);
  Serial.printf("ota_current_version=%s\n", kFirmwareVersion);
  Serial.printf("ota_latest_version=%s\n", availableVersion.c_str());
  if (updateAvailable) {
    Serial.println("ota_update_available=yes");
    Serial.println("ota_prompt=send_y_to_install_or_n_to_skip");
  } else {
    Serial.println("ota_status=up_to_date");
  }
}

void printUpdateProgress(int current, int total) {
  if (total <= 0) return;
  const int percent = static_cast<int>((100LL * current) / total);
  if (percent >= lastReportedPercent + 5 || percent == 100) {
    Serial.printf("ota_progress=%d%% bytes=%d/%d\n", percent, current, total);
    lastReportedPercent = percent;
  }
}

void installAvailableUpdate() {
  if (!updateAvailable || availableFirmwareUrl.isEmpty()) {
    Serial.println("ota_result=failed reason=no_update_selected");
    return;
  }

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
    Serial.println("ota_result=no_update");
  } else {
    Serial.printf("ota_result=failed code=%d message=%s\n",
                  httpUpdate.getLastError(),
                  httpUpdate.getLastErrorString().c_str());
  }
}

void printInstructions() {
  Serial.println("ota_instructions=send_c_to_check_releases");
  Serial.println("ota_instructions=send_y_to_install_or_n_to_skip");
}

}  // namespace

void setup() {
  fnk0104b::board.begin();
  fnk0104b::board.printStartupInfo("ota-demo", kFirmwareVersion);
  const esp_err_t validation = esp_ota_mark_app_valid_cancel_rollback();
  Serial.printf("ota_boot_validation=%s code=%d\n",
                validation == ESP_OK ? "accepted" : "error", validation);
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
      Serial.println("ota_update=skipped");
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
  if (!releaseChecked && WiFi.status() != WL_CONNECTED) {
    delay(20);
    return;
  }
  delay(20);
}
