#pragma once
#include "../../apps/codex-monitor/include/monitor_configuration.hpp"
#include <map>
#include <string>

struct ConfigStore {
  std::map<std::string, std::string> strings;
  std::map<std::string, unsigned> numbers;
  bool fail = false;
  bool isKey(const char* key) { return strings.count(key) || numbers.count(key); }
  std::string getString(const char* key, const char* fallback) { return strings.count(key) ? strings[key] : fallback; }
  size_t putString(const char* key, const char* value) { if (fail) return 0; strings[key] = value; return strlen(value); }
  uint8_t getUChar(const char* key, uint8_t fallback) { return numbers.count(key) ? numbers[key] : fallback; }
  uint16_t getUShort(const char* key, uint16_t fallback) { return numbers.count(key) ? numbers[key] : fallback; }
  bool getBool(const char* key, bool fallback) { return numbers.count(key) ? numbers[key] : fallback; }
  size_t putUChar(const char* key, uint8_t value) { if (fail) return 0; numbers[key] = value; return 1; }
  size_t putBool(const char* key, bool value) { return putUChar(key, value); }
  size_t putUShort(const char* key, uint16_t value) { if (fail) return 0; numbers[key] = value; return 2; }
};
void test_bridge_migration_survives_public_ota_and_explicit_clear() {
  ConfigStore store;
  monitor_config::Bridge legacy{"192.168.1.25", 8765, "private-test-key"};
  const auto migrated = monitor_config::loadBridge(store, legacy);
  TEST_ASSERT_EQUAL_STRING(legacy.host, migrated.host);
  TEST_ASSERT_TRUE(store.isKey("bridgeConfig"));
  // New public firmware has empty compiled defaults, but the same NVS contents.
  const auto publicBuild = monitor_config::loadBridge(store);
  TEST_ASSERT_EQUAL_STRING(legacy.token, publicBuild.token);
  TEST_ASSERT_EQUAL_STRING(legacy.host, publicBuild.host);
  TEST_ASSERT_EQUAL(8765, publicBuild.port);
  monitor_config::Bridge changed{"new-computer.local", 1234, "new-test-key"};
  TEST_ASSERT_TRUE(monitor_config::saveBridge(store, changed));
  TEST_ASSERT_EQUAL_STRING(changed.host, monitor_config::loadBridge(store, legacy).host);
  TEST_ASSERT_TRUE(monitor_config::saveBridge(store, {}));
  TEST_ASSERT_EQUAL_STRING("", monitor_config::loadBridge(store, legacy).host);
  TEST_ASSERT_EQUAL_STRING("", monitor_config::loadBridge(store, legacy).token);
}
void test_bridge_rejects_bad_records_and_storage_failures() {
  ConfigStore store;
  const monitor_config::Bridge legacy{"host.local", 8765, "test-key"};
  store.fail = true;
  TEST_ASSERT_EQUAL_STRING("", monitor_config::loadBridge(store, legacy).host);
  TEST_ASSERT_FALSE(store.isKey("bridgeConfig"));
  store.fail = false;
  for (const char* record : {"{broken", "{\"version\":2,\"host\":\"host.local\",\"port\":8765,\"token\":\"x\"}",
      "{\"version\":1,\"host\":\"bad/host\",\"port\":8765,\"token\":\"x\"}"}) {
    store.strings["bridgeConfig"] = record;
    TEST_ASSERT_EQUAL_STRING("", monitor_config::loadBridge(store, legacy).host);
    TEST_ASSERT_EQUAL_STRING(record, store.strings["bridgeConfig"].c_str());
  }
}
void test_bridge_request_validation_and_token_preservation() {
  monitor_config::Bridge current{"old.local", 8765, "kept-secret"}, next;
  StaticJsonDocument<1024> doc;
  const auto request = [&](const char* json) {
    doc.clear(); if (deserializeJson(doc, json)) return false;
    return monitor_config::bridgeRequest(doc.as<JsonVariantConst>(), current, next);
  };
  TEST_ASSERT_TRUE(request("{\"op\":\"save\",\"host\":\"192.168.1.9\",\"port\":1234}"));
  TEST_ASSERT_EQUAL_STRING("kept-secret", next.token);
  TEST_ASSERT_EQUAL(1234, next.port);
  TEST_ASSERT_TRUE(request("{\"op\":\"save\",\"host\":\"pc.local\",\"port\":1,\"token\":\"replacement\"}"));
  TEST_ASSERT_EQUAL_STRING("replacement", next.token);
  for (const char* invalid : {
      "{\"op\":\"save\",\"host\":\"http://pc.local\",\"port\":8765}",
      "{\"op\":\"save\",\"host\":\"pc.local\",\"port\":0}",
      "{\"op\":\"save\",\"host\":\"pc.local\",\"port\":65536}",
      "{\"op\":\"save\",\"host\":\"pc.local\",\"port\":1.5}",
      "{\"op\":\"save\",\"host\":\"pc.local\",\"port\":8765,\"token\":\"a\\r\\nb\"}",
      "{\"op\":\"save\",\"host\":\"pc.local\",\"port\":8765,\"token\":\"a\\u0000b\"}",
      "{\"op\":\"save\",\"host\":\"a..local\",\"port\":8765}",
      "{\"op\":\"save\",\"host\":\"\",\"port\":8765}", "[]"}) TEST_ASSERT_FALSE(request(invalid));
  ConfigStore bounded;
  monitor_config::Bridge maximum;
  memset(maximum.host, 'a', 127); maximum.host[127] = 0;
  memset(maximum.token, '\"', 128); maximum.token[128] = 0;
  maximum.port = 65535;
  TEST_ASSERT_TRUE(monitor_config::saveBridge(bounded, maximum));
  monitor_config::Bridge restored;
  TEST_ASSERT_TRUE(monitor_config::readBridge(bounded, restored));
  TEST_ASSERT_EQUAL_STRING(maximum.host, restored.host);
  TEST_ASSERT_EQUAL_STRING(maximum.token, restored.token);
  memset(maximum.token, 'a', sizeof(maximum.token));
  TEST_ASSERT_FALSE(monitor_config::validBridge(maximum));
  TEST_ASSERT_TRUE(request("{\"op\":\"clear\"}"));
  TEST_ASSERT_EQUAL_STRING("", next.host); TEST_ASSERT_EQUAL_STRING("", next.token);
}
void test_monitor_preferences_survive_restart_and_legacy_client_writes() {
  ConfigStore store;
  codex_hid::MonitorSettings desired{23, 87, 6, true, true, false, 321, 3, 2, 1, 47, 5432};
  TEST_ASSERT_TRUE(monitor_config::savePreferences(store, desired, 6));
  TEST_ASSERT_TRUE(monitor_config::savePhotos(store, {2, 81, 17}));
  auto restored = monitor_config::loadPreferences(store);
  TEST_ASSERT_EQUAL(23, restored.volume); TEST_ASSERT_EQUAL(87, restored.timeout);
  TEST_ASSERT_EQUAL(6, restored.slots); TEST_ASSERT_TRUE(restored.separateVoice); TEST_ASSERT_TRUE(restored.avatars);
  TEST_ASSERT_FALSE(restored.ledEnabled); TEST_ASSERT_EQUAL(321, restored.ledSeconds);
  TEST_ASSERT_EQUAL(3, restored.ledWorkingPattern); TEST_ASSERT_EQUAL(2, restored.ledCompletePattern);
  TEST_ASSERT_EQUAL(1, restored.ledAttentionPattern); TEST_ASSERT_EQUAL(47, restored.ledBrightness);
  TEST_ASSERT_EQUAL(5432, restored.ledPeriodMs);
  const auto photos = monitor_config::loadPhotos(store);
  TEST_ASSERT_EQUAL(2, photos.mode); TEST_ASSERT_EQUAL(81, photos.delay); TEST_ASSERT_EQUAL(17, photos.interval);
  // A four-byte settings client must not reset newer saved preferences.
  codex_hid::MonitorSettings oldClient;
  oldClient.volume = 70; oldClient.timeout = 15;
  TEST_ASSERT_TRUE(monitor_config::savePreferences(store, oldClient, 1));
  restored = monitor_config::loadPreferences(store);
  TEST_ASSERT_EQUAL(70, restored.volume); TEST_ASSERT_EQUAL(15, restored.timeout);
  TEST_ASSERT_EQUAL(6, restored.slots); TEST_ASSERT_TRUE(restored.separateVoice); TEST_ASSERT_TRUE(restored.avatars);
  TEST_ASSERT_EQUAL(321, restored.ledSeconds); TEST_ASSERT_EQUAL(3, restored.ledWorkingPattern);
  TEST_ASSERT_EQUAL(5432, restored.ledPeriodMs); TEST_ASSERT_EQUAL(2, monitor_config::loadPhotos(store).mode);
  // Partial existing records retain known fields and supply defaults only for absent keys.
  ConfigStore older; older.numbers["volume"] = 12; older.numbers["timeout"] = 99;
  restored = monitor_config::loadPreferences(older);
  TEST_ASSERT_EQUAL(12, restored.volume); TEST_ASSERT_EQUAL(99, restored.timeout);
  TEST_ASSERT_EQUAL(60, restored.ledSeconds);
}
