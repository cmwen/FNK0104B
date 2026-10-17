#pragma once
#include <ArduinoJson.h>
#include <codex_hid/monitor_settings.hpp>
#include <ui/idle_photos.hpp>
#include <cstring>

namespace monitor_config {
struct Bridge {
  char host[128]{};
  uint16_t port = 8765;
  char token[129]{};
};
// Hostnames/IPv4 only: no URL, path, userinfo or HTTP header characters.
inline bool validBridge(const Bridge& value) {
  const size_t host = strnlen(value.host, sizeof(value.host));
  const size_t token = strnlen(value.token, sizeof(value.token));
  if (host == sizeof(value.host) || token == sizeof(value.token) || !value.port) return false;
  if (!host) return token == 0;
  if (value.host[0] == '.' || value.host[0] == '-' || value.host[host - 1] == '.' || value.host[host - 1] == '-') return false;
  for (size_t i = 0; i < host; ++i) {
    const char c = value.host[i];
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9') || c == '.' || c == '-')) return false;
    if (c == '.' && i && (value.host[i - 1] == '.' || value.host[i - 1] == '-')) return false;
    if (c == '-' && i && value.host[i - 1] == '.') return false;
  }
  for (size_t i = 0; i < token; ++i)
    if (value.token[i] < 33 || value.token[i] > 126) return false;
  return true;
}
inline bool decodeBridge(JsonVariantConst object, Bridge& value) {
  if (!object.is<JsonObjectConst>() || !object["version"].is<unsigned>() || object["version"] != 1 ||
      !object["host"].is<const char*>() || !object["token"].is<const char*>() ||
      !object["port"].is<unsigned>() || object["port"].as<unsigned>() > 65535) return false;
  const JsonString host = object["host"].as<JsonString>(), token = object["token"].as<JsonString>();
  if (host.size() >= sizeof(value.host) || token.size() >= sizeof(value.token) ||
      strlen(host.c_str()) != host.size() || strlen(token.c_str()) != token.size()) return false;
  Bridge next;
  memcpy(next.host, host.c_str(), host.size() + 1);
  memcpy(next.token, token.c_str(), token.size() + 1);
  next.port = object["port"].as<unsigned>();
  if (!validBridge(next)) return false;
  value = next;
  return true;
}
inline bool bridgeRequest(JsonVariantConst request, const Bridge& current, Bridge& next) {
  if (!request.is<JsonObjectConst>()) return false;
  const char* operation = request["op"] | "";
  if (!strcmp(operation, "clear")) { next = Bridge{}; return true; }
  if (strcmp(operation, "save")) return false;
  // Reuse the stored-record validator; an omitted token preserves the saved key.
  StaticJsonDocument<768> record;
  record["version"] = 1; record["host"] = request["host"]; record["port"] = request["port"];
  if (request.as<JsonObjectConst>().containsKey("token")) record["token"] = request["token"];
  else record["token"] = current.token;
  return !record.overflowed() && decodeBridge(record.as<JsonVariantConst>(), next) && next.host[0];
}
template<class Store> bool saveBridge(Store& store, const Bridge& value) {
  if (!validBridge(value)) return false;
  StaticJsonDocument<512> doc;
  doc["version"] = 1; doc["host"] = value.host; doc["port"] = value.port; doc["token"] = value.token;
  char record[512];
  if (doc.overflowed() || measureJson(doc) >= sizeof(record)) return false;
  const size_t bytes = serializeJson(doc, record, sizeof(record));
  return store.putString("bridgeConfig", record) == bytes;
}
template<class Store> bool readBridge(Store& store, Bridge& value) {
  const auto record = store.getString("bridgeConfig", "");
  StaticJsonDocument<768> doc;
  return !deserializeJson(doc, record.c_str()) && decodeBridge(doc.as<JsonVariantConst>(), value);
}
template<class Store> Bridge loadBridge(Store& store, const Bridge& legacy = {}) {
  Bridge saved;
  // A saved record, including an explicit clear, always outranks compiled defaults.
  // An invalid/newer record fails closed and is never overwritten by migration.
  if (store.isKey("bridgeConfig")) { readBridge(store, saved); return saved; }
  if (legacy.host[0] && validBridge(legacy) && saveBridge(store, legacy)) return legacy;
  return saved;
}

// Existing NVS names are the persistence contract across OTA and old BLE clients.
template<class Store> codex_hid::MonitorSettings loadPreferences(Store& p) {
  codex_hid::MonitorSettings s;
  s.volume = p.getUChar("volume", 50); if (s.volume > 100) s.volume = 50;
  s.timeout = p.getUShort("timeout", 30); if (s.timeout < 1 || s.timeout > 120) s.timeout = 30;
  s.slots = p.getUChar("microSlots", 3) == 6 ? 6 : 3;
  s.separateVoice = p.getBool("usbVoice", false); s.avatars = p.getBool("microAvatars", false);
  s.ledEnabled = p.getBool("statusLed", true);
  s.ledSeconds = p.getUShort("ledSeconds", 60); if (s.ledSeconds < 5 || s.ledSeconds > 600) s.ledSeconds = 60;
  s.ledWorkingPattern = p.getUChar("ledWorking", 0); if (s.ledWorkingPattern > 3) s.ledWorkingPattern = 0;
  s.ledCompletePattern = p.getUChar("ledComplete", 0); if (s.ledCompletePattern > 3) s.ledCompletePattern = 0;
  s.ledAttentionPattern = p.getUChar("ledAttention", 0); if (s.ledAttentionPattern > 3) s.ledAttentionPattern = 0;
  s.ledBrightness = p.getUChar("ledBrightness", 6); if (s.ledBrightness < 1 || s.ledBrightness > 100) s.ledBrightness = 6;
  s.ledPeriodMs = p.getUShort("ledPeriod", 2000); if (s.ledPeriodMs < 500 || s.ledPeriodMs > 10000) s.ledPeriodMs = 2000;
  return s;
}
template<class Store> bool savePreferences(Store& p, const codex_hid::MonitorSettings& s, unsigned version) {
  bool ok = p.putUChar("volume", s.volume) == 1;
  ok = (p.putUShort("timeout", s.timeout) == 2) && ok;
  if (version >= 2) ok = (p.putUChar("microSlots", s.slots) == 1) && ok;
  if (version >= 3) ok = (p.putBool("usbVoice", s.separateVoice) == 1) && ok;
  if (version >= 4) ok = (p.putBool("microAvatars", s.avatars) == 1) && ok;
  if (version >= 5) {
    ok = (p.putBool("statusLed", s.ledEnabled) == 1) && ok;
    ok = (p.putUShort("ledSeconds", s.ledSeconds) == 2) && ok;
  }
  if (version >= 6) {
    ok = (p.putUChar("ledWorking", s.ledWorkingPattern) == 1) && ok;
    ok = (p.putUChar("ledComplete", s.ledCompletePattern) == 1) && ok;
    ok = (p.putUChar("ledAttention", s.ledAttentionPattern) == 1) && ok;
    ok = (p.putUChar("ledBrightness", s.ledBrightness) == 1) && ok;
    ok = (p.putUShort("ledPeriod", s.ledPeriodMs) == 2) && ok;
  }
  return ok;
}
template<class Store> ui::photos::Settings loadPhotos(Store& p) {
  ui::photos::Settings s{p.getUChar("idleMode", 0), p.getUShort("photoDelay", 60), p.getUShort("photoInterval", 10)};
  uint8_t data[] = {1, s.mode, uint8_t(s.delay), uint8_t(s.delay >> 8), uint8_t(s.interval), uint8_t(s.interval >> 8)};
  ui::photos::Settings checked;
  return ui::photos::decode(data, sizeof(data), checked) ? checked : ui::photos::Settings{};
}
template<class Store> bool savePhotos(Store& p, const ui::photos::Settings& s) {
  bool ok = p.putUChar("idleMode", s.mode) == 1;
  ok = (p.putUShort("photoDelay", s.delay) == 2) && ok;
  return (p.putUShort("photoInterval", s.interval) == 2) && ok;
}
} // namespace monitor_config
