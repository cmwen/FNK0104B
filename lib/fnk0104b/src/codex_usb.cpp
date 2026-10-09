#if defined(FNK0104B_ENABLE_CODEX_HID)
#include <Arduino.h>
#include <USB.h>
#include <USBHID.h>
#include <tusb.h>
#include <atomic>
#include <freertos/queue.h>
#include <fnk0104b/codex_usb.hpp>
#include <codex_hid/wire.hpp>

namespace fnk0104b::codex_usb {
namespace {
QueueHandle_t rx = nullptr;
codex_hid::ReceiveEpochs generations;
std::atomic<uint32_t> lost{0};
class Vendor : public USBHIDDevice {
 public:
  USBHID hid;
  bool registered = false;
  Vendor() { registered = USBHID::addDevice(this, sizeof(codex_hid::kDescriptor)); }
  uint16_t _onGetDescriptor(uint8_t* destination) override {
    memcpy(destination, codex_hid::kDescriptor, sizeof(codex_hid::kDescriptor));
    return sizeof(codex_hid::kDescriptor);
  }
  void _onOutput(uint8_t id, const uint8_t* data, uint16_t length) override {
    if (id != codex_hid::kReportId || !data || length < 2 || length > 63 || !rx) {
      ++lost; generations.overflow(); return;
    }
    Report report{};
    memcpy(report.body, data, length); report.length = length;
    report.epoch = generations.connection(); report.fragments = generations.fragments();
    if (xQueueSend(rx, &report, 0) != pdTRUE) { ++lost; generations.overflow(); }
  }
  // Arduino 3.3.12 routes SET_REPORT with explicit ID through this hook too.
  void _onSetFeature(uint8_t id, const uint8_t* data, uint16_t length) override {
    _onOutput(id, data, length);
  }
};
Vendor vendor;
void event(void*, esp_event_base_t, int32_t id, void*) {
  if (id == ARDUINO_USB_STARTED_EVENT || id == ARDUINO_USB_STOPPED_EVENT) {
    generations.reconnect();
  }
}
}
bool begin() {
  rx = xQueueCreate(16, sizeof(Report));
  if (!rx) return false;
  if (!vendor.registered) { vQueueDelete(rx); rx = nullptr; return false; }
  // Identity is compiled into the core; CDC auto-starts USB before setup().
  USB.onEvent(event);
  vendor.hid.begin();
  return USB.begin();
}
bool mounted() { return tud_mounted(); }
uint32_t epoch() { return generations.connection(); }
uint32_t dropped() { return lost.load(); }
bool receive(Report& report) { return rx && xQueueReceive(rx, &report, 0) == pdTRUE; }
bool send(const uint8_t* body, size_t length) {
  // The backend is the sole writer. Advance framing only when TinyUSB accepts
  // the packet, rather than retrying a packet after a completion-wait timeout.
  return body && length == codex_hid::kBodySize && mounted() && vendor.hid.ready() &&
    tud_hid_report(codex_hid::kReportId, body, length);
}
}
#endif
