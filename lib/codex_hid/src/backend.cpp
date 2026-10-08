#if defined(FNK0104B_ENABLE_CODEX_HID)
#include <Arduino.h>
#include <atomic>
#include <fnk0104b/codex_usb.hpp>
#include <codex_hid/backend.hpp>
#include <codex_hid/ble_backend.hpp>
#include <freertos/queue.h>
#include <esp_heap_caps.h>
#include <new>

namespace codex_hid {
namespace {
namespace usb = fnk0104b::codex_usb;
struct ProtocolState { Decoder decoder; Protocol protocol; };
#if defined(FNK0104B_ENABLE_USB_AUDIO)
ProtocolState* state = nullptr;  // CPU-only JSON/framing state, never USB DMA.
#else
ProtocolState ownedState;
ProtocolState* state = &ownedState;
#endif
struct Input { uint32_t epoch; uint8_t key; bool pressed; bool tap; };
std::atomic<bool> discovered{false};
std::atomic<uint32_t> lastProtocol{0};
struct Reply { char json[1024]; uint16_t length; bool discovery; };
#if defined(FNK0104B_ENABLE_USB_AUDIO)
StaticQueue_t replyControl;
#endif
QueueHandle_t statusQueue = nullptr, inputQueue = nullptr, replyQueue = nullptr;
char tx[1024]{};
size_t txLength = 0, txOffset = 0;
uint32_t txStarted = 0;
bool releasePending = false;
uint8_t releaseKey = 0;
bool txDiscovery = false;
uint32_t currentEpoch = 0;

void message(const char* json, size_t size, void*) {
  static Reply reply;
  reply.length = state->protocol.process(json, size, reply.json, sizeof(reply.json));
  reply.discovery = state->protocol.valid && (!strcmp(state->protocol.method, "device.status") ||
    !strcmp(state->protocol.method, "sys.version") || !strcmp(state->protocol.method, "v.oai.thstatus"));
  if (reply.length && xQueueSend(replyQueue, &reply, 0) != pdTRUE)
    Serial.println("codex_hid reply=queue_full");
  Serial.printf("codex_hid protocol=%s method=%s reply_bytes=%u\n",
    state->protocol.valid ? "decoded" : "invalid", state->protocol.method, unsigned(reply.length));
  if (state->protocol.valid && !strcmp(state->protocol.method, "v.oai.thstatus")) {
    xQueueOverwrite(statusQueue, &state->protocol.status);
    Serial.printf("codex_hid status_received revision=%lu\n", (unsigned long)state->protocol.status.revision);
  }
}
void worker(void*) {
  bool wasMounted = false, active = false;
  uint32_t lastRx = 0, loggedDrops = 0, lastReportLog = 0, reports = 0;
  for (;;) {
    ble::poll();
    const uint32_t epoch = usb::epoch();
    const bool mounted = usb::mounted();
    if (epoch != currentEpoch || mounted != wasMounted) {
      currentEpoch = epoch; wasMounted = mounted;
      state->decoder.reset(); txLength = txOffset = 0; releasePending = false; active = false;
      discovered = false;
      state->protocol.status = Status{};
      xQueueOverwrite(statusQueue, &state->protocol.status);
      xQueueReset(inputQueue); xQueueReset(replyQueue);
      Serial.printf("codex_hid usb=%s epoch=%lu\n", mounted ? "mounted" : "unmounted", (unsigned long)epoch);
    }
    if (active && millis() - lastRx > 10000) {
      active = false; Serial.println("codex_hid host=idle");
    }
    if (millis() - lastRx > 2000) state->decoder.reset();
    const uint32_t drops = usb::dropped();
    if (drops != loggedDrops) {
      loggedDrops = drops; state->decoder.reset();
      Serial.printf("codex_hid reports_dropped=%lu\n", (unsigned long)drops);
    }
    if (txLength) {
      uint8_t body[kBodySize];
      const size_t count = frame(tx, txLength, txOffset, body);
      if (usb::send(body, sizeof(body))) {
        txOffset += count;
        if (txOffset == txLength + 2) {
          if (txDiscovery) { discovered = true; lastProtocol = millis(); txDiscovery = false; }
          Serial.println("codex_hid tx=complete report_id=6"); txLength = 0;
        }
      } else if (millis() - txStarted > 500) {
        Serial.println("codex_hid tx=retry"); txStarted = millis();
      }
    } else if (releasePending) {
      txDiscovery = false;
      txLength = keyEvent(releaseKey, false, tx, sizeof(tx)); txOffset = 0; txStarted = millis(); releasePending = false;
    } else {
      static Reply reply;
      if (xQueueReceive(replyQueue, &reply, 0) == pdTRUE) {
        txDiscovery = reply.discovery;
        memcpy(tx, reply.json, reply.length); txLength = reply.length; txOffset = 0; txStarted = millis();
        continue;
      }
      Input input{};
      if (mounted && xQueueReceive(inputQueue, &input, 0) == pdTRUE && input.epoch == epoch) {
        txDiscovery = false;
        txLength = keyEvent(input.key, input.pressed, tx, sizeof(tx));
        txOffset = 0; txStarted = millis(); releasePending = input.tap; releaseKey = input.key;
        Serial.printf("codex_hid key=%u action=%s\n", input.key, input.pressed ? "press" : "release");
      } else {
        usb::Report report{};
        // One report per pass, keeping callbacks and UI independent of host floods.
        if (usb::receive(report) && mounted && report.epoch == epoch) {
          lastRx = millis();
          if (!active) { active = true; Serial.println("codex_hid host=active"); }
          ++reports;
          if (millis() - lastReportLog >= 1000) {
            lastReportLog = millis();
            Serial.printf("codex_hid rx report_id=6 bytes=%u reports=%lu\n", report.length, (unsigned long)reports);
          }
          if (!state->decoder.feed(report.body, report.length, message, nullptr))
            Serial.println("codex_hid framing=invalid_or_oversized");
        }
      }
    }
    vTaskDelay(pdMS_TO_TICKS(2));
  }
}
}
bool begin() {
#if defined(FNK0104B_ENABLE_USB_AUDIO)
  void* storage = heap_caps_malloc(sizeof(ProtocolState), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!storage) return false;
  state = new (storage) ProtocolState{};
#endif
  statusQueue = xQueueCreate(1, sizeof(Status));
  inputQueue = xQueueCreate(8, sizeof(Input));
#if defined(FNK0104B_ENABLE_USB_AUDIO)
  // Reports are copied by tasks, never used as USB DMA buffers or from an ISR.
  auto* replyStorage = static_cast<uint8_t*>(heap_caps_malloc(4 * sizeof(Reply), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  replyQueue = replyStorage ? xQueueCreateStatic(4, sizeof(Reply), replyStorage, &replyControl) : nullptr;
#else
  replyQueue = xQueueCreate(4, sizeof(Reply));
#endif
  if (!statusQueue || !inputQueue || !replyQueue || !usb::begin()) return false;
  return xTaskCreate(worker, "codex-hid", 8192, nullptr, 1, nullptr) == pdPASS;
}
bool tap(uint8_t id) {
  if (transport() == Transport::Ble) return ble::key(id, true, true);
  const Input input{usb::epoch(), id, true, true};
  return id <= 16 && inputQueue && usb::mounted() && xQueueSend(inputQueue, &input, 0) == pdTRUE;
}
bool agent0Tap() { return tap(0); }
LinkState usbLinkState() {
  if (!usb::mounted()) return LinkState::Off;
  if (!discovered.load()) return LinkState::Usb;
  return millis() - lastProtocol.load() < 60000 ? LinkState::Linked : LinkState::Idle;
}
bool key(uint8_t id, bool pressed) {
  if (transport() == Transport::Ble) return ble::key(id, pressed);
  const Input input{usb::epoch(), id, pressed, false};
  return id <= 16 && inputQueue && microConnected(linkState()) && xQueueSend(inputQueue, &input, 0) == pdTRUE;
}
bool microphoneKey(bool pressed) { return key(10, pressed); }
bool takeStatus(Status& status) {
  if (transport() == Transport::Ble) return ble::takeStatus(status);
  return statusQueue && xQueueReceive(statusQueue, &status, 0) == pdTRUE;
}
Transport transport() {
  return chooseTransport(usbLinkState(), ble::linkState());
}
LinkState linkState() { return transport() == Transport::Ble ? ble::linkState() : usbLinkState(); }
}
#endif
