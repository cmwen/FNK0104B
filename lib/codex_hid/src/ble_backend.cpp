#if defined(FNK0104B_ENABLE_CODEX_BLE)
#include <Arduino.h>
#include <atomic>
#include <new>
#include <esp_heap_caps.h>
#include <freertos/queue.h>
#include <fnk0104b/codex_ble.hpp>
#include <codex_hid/ble_backend.hpp>

namespace codex_hid::ble {
namespace {
namespace radio = fnk0104b::codex_ble;
struct Reply { char json[1024]; size_t size; bool discovery; };
struct Input { uint32_t epoch; uint8_t id; bool pressed, tap; };
struct State {
  Decoder decoder;
  Protocol protocol;
  QueueHandle_t replies, inputs, statuses;
  StaticQueue_t replyControl;
  uint8_t replyStorage[4 * sizeof(Reply)]; // CPU-only queue storage in PSRAM.
  char tx[1024]{};
  size_t size = 0, offset = 0;
  uint32_t epoch = 0, started = 0, lastRx = 0;
  bool discovery = false, release = false;
  uint8_t releaseKey = 0;
};
std::atomic<State*> state{nullptr};
std::atomic<bool> discovered{false};
std::atomic<uint32_t> lastProtocol{0};
void message(const char* json, size_t size, void* context) {
  auto& s = *static_cast<State*>(context);
  Reply reply{};
  reply.size = s.protocol.process(json, size, reply.json, sizeof(reply.json));
  reply.discovery = s.protocol.valid && (!strcmp(s.protocol.method, "device.status") ||
    !strcmp(s.protocol.method, "sys.version") || !strcmp(s.protocol.method, "v.oai.thstatus"));
  if (reply.size) xQueueSend(s.replies, &reply, 0);
  if (s.protocol.valid && !strcmp(s.protocol.method, "v.oai.thstatus"))
    xQueueOverwrite(s.statuses, &s.protocol.status);
  Serial.printf("codex_ble protocol=%s reply=%u\n", s.protocol.method, unsigned(reply.size));
}
}
bool begin() {
  if (state) return true;
  auto* storage = heap_caps_malloc(sizeof(State), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!storage) return false;
  auto* s = new (storage) State{};
  s->replies = xQueueCreateStatic(4, sizeof(Reply), s->replyStorage, &s->replyControl);
  s->inputs = xQueueCreate(8, sizeof(Input));
  s->statuses = xQueueCreate(1, sizeof(Status));
  if (!s->replies || !s->inputs || !s->statuses) {
    if (s->replies) vQueueDelete(s->replies);
    if (s->inputs) vQueueDelete(s->inputs);
    if (s->statuses) vQueueDelete(s->statuses);
    s->~State(); free(storage); return false;
  }
  state = s;
  return true;
}
void poll() {
  auto* ptr = state.load();
  if (!ptr) return;
  auto& s = *ptr;
  const uint32_t epoch = radio::epoch();
  if (epoch != s.epoch) {
    s.epoch = epoch; s.decoder.reset(); s.size = s.offset = 0; s.release = false;
    discovered = false; s.protocol.status = Status{};
    xQueueReset(s.replies); xQueueReset(s.inputs);
    xQueueOverwrite(s.statuses, &s.protocol.status);
  }
  if (!radio::mounted()) return;
  radio::Report report{};
  if (radio::receive(report) && report.epoch == epoch) {
    s.lastRx = millis(); s.decoder.feed(report.body, report.length, message, &s);
  } else if (millis() - s.lastRx > 2000) s.decoder.reset();
  if (s.size) {
    uint8_t body[kBodySize];
    const size_t count = frame(s.tx, s.size, s.offset, body);
    if (radio::send(body, sizeof(body))) {
      s.offset += count;
      if (s.offset == s.size + 2) {
        if (s.discovery) { discovered = true; lastProtocol = millis(); }
        s.size = 0;
      }
    } else if (millis() - s.started > 500) {
      s.started = millis(); // Retain the frame and any paired key release.
      Serial.println("codex_ble tx=retry check=subscription_or_mtu");
    }
    return;
  }
  if (s.release) {
    s.size = keyEvent(s.releaseKey, false, s.tx, sizeof(s.tx));
    s.release = false; s.discovery = false;
  } else {
    Reply reply{};
    Input input{};
    if (xQueueReceive(s.replies, &reply, 0) == pdTRUE) {
      memcpy(s.tx, reply.json, reply.size); s.size = reply.size; s.discovery = reply.discovery;
    } else if (xQueueReceive(s.inputs, &input, 0) == pdTRUE && input.epoch == epoch) {
      s.size = keyEvent(input.id, input.pressed, s.tx, sizeof(s.tx));
      s.releaseKey = input.id; s.release = input.tap; s.discovery = false;
    }
  }
  s.offset = 0; s.started = millis();
}
LinkState linkState() {
  if (!radio::mounted()) return LinkState::Off;
  if (!discovered) return LinkState::Usb; // Physical link, no Micro session yet.
  return millis() - lastProtocol.load() < 60000 ? LinkState::Linked : LinkState::Idle;
}
bool key(uint8_t id, bool pressed, bool tap) {
  auto* s = state.load();
  const Input input{radio::epoch(), id, pressed, tap};
  return s && id <= 12 && microConnected(linkState()) && xQueueSend(s->inputs, &input, 0) == pdTRUE;
}
bool takeStatus(Status& status) {
  auto* s = state.load();
  return s && xQueueReceive(s->statuses, &status, 0) == pdTRUE;
}
}
#endif
