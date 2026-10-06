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
bool microphoneKey(bool pressed);
bool agent0Tap(); // Explicit diagnostic input only; queues press and release together.
bool takeStatus(Status& status); // Called by the application, independent of Wi-Fi state.
}
