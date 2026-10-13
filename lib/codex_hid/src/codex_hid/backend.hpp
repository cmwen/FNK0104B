#pragma once
#include <codex_hid/protocol.hpp>
namespace codex_hid {
bool begin();
enum class LinkState : uint8_t { Off, Usb, Linked, Idle };
// USB enumeration alone does not establish a Desktop Micro connection.
constexpr bool microConnected(LinkState state) {
  return state == LinkState::Linked || state == LinkState::Idle;
}
LinkState linkState();
enum class Transport : uint8_t { None, Usb, Ble };
constexpr Transport chooseTransport(LinkState usb, LinkState ble) {
  return microConnected(usb) ? Transport::Usb : microConnected(ble) ? Transport::Ble : Transport::None;
}
LinkState usbLinkState();
Transport transport();
bool microphoneKey(bool pressed);
bool key(uint8_t id, bool pressed);
bool tap(uint8_t id);
bool agent0Tap(); // Explicit diagnostic input only; queues press and release together.
// Update from the selected transport's retained mailbox, independent of Wi-Fi.
// Returns false if no snapshot exists or the caller already has the same values.
bool takeStatus(Status& status);
}
