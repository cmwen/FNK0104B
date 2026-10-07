#pragma once
#include <codex_hid/backend.hpp>
namespace codex_hid::ble {
#if defined(FNK0104B_ENABLE_CODEX_BLE)
bool begin();
void poll(); // Run from the existing HID worker, never from a BLE callback.
LinkState linkState();
bool key(uint8_t id, bool pressed, bool tap = false);
bool takeStatus(Status& status);
#else
inline bool begin() { return false; }
inline void poll() {}
inline LinkState linkState() { return LinkState::Off; }
inline bool key(uint8_t, bool, bool = false) { return false; }
inline bool takeStatus(Status&) { return false; }
#endif
}
