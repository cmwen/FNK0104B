#if defined(FNK0104B_ENABLE_USB_HID)
#include <Arduino.h>
#include <USB.h>
#include <USBHIDKeyboard.h>
#include <tusb.h>
#include <atomic>
#include <fnk0104b/keyboard.hpp>

#if ARDUINO_USB_MODE != 0
#error "FNK0104B HID requires USB-OTG mode (ARDUINO_USB_MODE=0)"
#endif
namespace fnk0104b {
namespace {
USBHIDKeyboard device;
USBHID transport;
std::atomic<bool> num_lock{false};
bool waitForEndpoint() {
  const uint32_t started = millis();
  while (tud_ready() && !transport.ready() && millis() - started < 250)
    delay(1);
  return tud_ready() && transport.ready();
}
bool send(const KeyReport& report) {
  if (!waitForEndpoint()) {
    Serial.printf("hid_send=endpoint_unavailable mounted=%d suspended=%d\n", tud_mounted(), tud_suspended());
    return false;
  }
  const bool sent = transport.SendReport(HID_REPORT_ID_KEYBOARD, &report, sizeof(report), 250);
  if (!sent) Serial.println("hid_send=transfer_failed");
  return sent;
}
void ledEvent(void*, esp_event_base_t, int32_t, void* data) {
  num_lock.store(static_cast<arduino_usb_hid_keyboard_event_data_t*>(data)->numlock);
}
}
KeyboardSupport keyboard;
bool KeyboardSupport::begin() {
  device.onEvent(ARDUINO_USB_HID_KEYBOARD_LED_EVENT, ledEvent);
  device.begin();
  return USB.begin();
}
// Endpoint readiness drops while a report is in flight; it is not connection state.
bool KeyboardSupport::ready() { return tud_ready(); }
bool KeyboardSupport::endpointReady() { return transport.ready(); }
bool KeyboardSupport::releaseAll() {
  const KeyReport report = {};
  return send(report);
}
bool KeyboardSupport::tap(uint8_t usage, uint8_t modifiers) {
  if (!ready()) return false;
  KeyReport report = {};
  report.modifiers = modifiers;
  report.keys[0] = usage;
  const bool pressed = send(report);
  delay(12);
  const bool released = releaseAll();
  delay(12);
  return pressed && released;
}
bool KeyboardSupport::text(const char* ascii) {
  if (!ascii) return false;
  for (; *ascii; ++ascii) {
    uint8_t usage = 0;
    if (*ascii >= 'a' && *ascii <= 'z') usage = 0x04 + *ascii - 'a';
    else if (*ascii >= '1' && *ascii <= '9') usage = 0x1e + *ascii - '1';
    else if (*ascii == '0') usage = 0x27;
    else if (*ascii == ' ') usage = 0x2c;
    else return false;
    if (!tap(usage)) return false;
  }
  return true;
}
bool KeyboardSupport::numLock() const { return num_lock.load(); }
}
#endif
