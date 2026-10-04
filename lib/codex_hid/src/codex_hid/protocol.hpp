#pragma once
#include <ArduinoJson.h>
#include <codex_hid/wire.hpp>

namespace codex_hid {
struct Slot {
  bool present = false;
  uint32_t color = 0;
  float brightness = 0, speed = 0;
  char effect[16]{}; // Retain host value; do not infer semantic agent states.
};
struct Status { Slot slots[6]; uint32_t revision = 0; };
class Protocol {
 public:
  Status status;
  char method[48]{};
  bool valid = false;
  size_t process(const char* input, size_t length, char* output, size_t capacity) {
    valid = false; method[0] = 0;
    request_.clear(); reply_.clear();
    if (deserializeJson(request_, input, length, DeserializationOption::NestingLimit(12)) ||
        !request_.is<JsonObject>()) return 0;
    const char* name = request_["m"].as<const char*>();
    if (!name) name = request_["method"].as<const char*>();
    if (!name || strlen(name) >= sizeof(method)) return 0;
    strlcpy_local(method, name, sizeof(method)); valid = true;
    JsonVariantConst params = request_.containsKey("p") ? request_["p"] : request_["params"];
    const bool hasId = request_.containsKey("id") && !request_["id"].isNull();
    if (hasId && !(request_["id"].is<const char*>() || request_["id"].is<int64_t>())) {
      valid = false; return 0;
    }
    bool known = true, notificationAck = false;
    if (!strcmp(method, "device.status") || !strcmp(method, "sys.version")) {
      JsonObject result = reply_.createNestedObject("result");
      result["version"] = "0.1.0-fnk0104b-hid";
      if (!strcmp(method, "device.status")) {
        result["profile_index"] = 0; result["layer_index"] = 1;
        // USB-powered compatibility placeholder, not a battery measurement.
        result["battery"] = 100; result["is_charging"] = false;
      }
    } else if (!strcmp(method, "v.oai.thstatus")) {
      if (!params.is<JsonArrayConst>()) { valid = false; return 0; }
      for (JsonObjectConst item : params.as<JsonArrayConst>()) {
        if (!item["id"].is<unsigned>() || item["id"].as<unsigned>() >= 6) continue;
        Slot& slot = status.slots[item["id"].as<unsigned>()];
        slot.present = true;
        if (item["c"].is<uint32_t>() && item["c"].as<uint32_t>() <= 0xffffff)
          slot.color = item["c"].as<uint32_t>();
        if (item["b"].is<float>()) slot.brightness = bounded(item["b"].as<float>(), 1);
        if (item["s"].is<float>()) slot.speed = bounded(item["s"].as<float>(), 10);
        if (item["e"].is<const char*>()) strlcpy_local(slot.effect, item["e"], sizeof(slot.effect));
        else if (item["e"].is<int>()) snprintf(slot.effect, sizeof(slot.effect), "%d", item["e"].as<int>());
      }
      ++status.revision;
      reply_.createNestedObject("result")["ok"] = 1; notificationAck = true;
    } else if (!strcmp(method, "v.oai.rgbcfg") || !strcmp(method, "lights.preview") ||
               !strcmp(method, "host.focused_app")) {
      // Runtime compatibility ACK only; no persistent configuration or RGB hardware changes.
      reply_.createNestedObject("result")["ok"] = 1; notificationAck = true;
    } else {
      known = false;
      JsonObject error = reply_.createNestedObject("error");
      error["code"] = -32601; error["message"] = "Method not found";
    }
    if (!hasId && (!known || !notificationAck)) return 0;
    if (hasId) reply_["id"] = request_["id"];
    else reply_["id"] = nullptr;
    reply_["method"] = method;
    if (reply_.overflowed() || measureJson(reply_) >= capacity) return 0;
    return serializeJson(reply_, output, capacity);
  }
 private:
  StaticJsonDocument<4096> request_;
  StaticJsonDocument<768> reply_;
  static float bounded(float n, float upper) { return n >= 0 ? (n < upper ? n : upper) : 0; }
  static void strlcpy_local(char* dst, const char* src, size_t n) {
    if (!n) return;
    const size_t count = strlen(src) < n - 1 ? strlen(src) : n - 1;
    memcpy(dst, src, count); dst[count] = 0;
  }
};
inline size_t agentEvent(bool pressed, char* output, size_t capacity) {
  const int n = snprintf(output, capacity,
    "{\"m\":\"v.oai.hid\",\"p\":{\"k\":\"AG00\",\"act\":%u,\"ag\":0}}", pressed ? 1 : 0);
  return n > 0 && size_t(n) < capacity ? size_t(n) : 0;
}
inline size_t microphoneEvent(bool pressed, char* output, size_t capacity) {
  const int n = snprintf(output, capacity,
    "{\"m\":\"v.oai.hid\",\"p\":{\"k\":\"ACT10\",\"act\":%u}}", pressed ? 1 : 0);
  return n > 0 && size_t(n) < capacity ? size_t(n) : 0;
}

}
