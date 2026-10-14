#pragma once
#include <codex_hid/protocol.hpp>
#include <ui/avatar_assets.hpp>
#include <ui/monitor_theme.hpp>

namespace ui::micro {
// The LCD retains agent information separately from the host's LED lighting.
// Off/black is not a semantic idle update; white/green and other nonzero
// colors still replace the last displayed state. Silence needs no update.
inline void updateAgentDisplay(const codex_hid::Status& lighting, codex_hid::Status& displayed) {
  bool present = false;
  for (const auto& slot : lighting.slots) present |= slot.present;
  if (!present && lighting.revision == 0) {
    displayed = {}; // The backend explicitly invalidated the connection.
    return;
  }
  displayed.revision = lighting.revision;
  for (unsigned i = 0; i < 6; ++i) {
    const auto& slot = lighting.slots[i];
    if (!slot.present || !slot.color) continue;
    displayed.slots[i] = slot;
    displayed.slots[i].brightness = 1; // LED intensity does not dim agent information.
  }
}

// Desktop lighting colors observed in the protocol research linked in the docs.
// Effects indicate selection/pulsing, not whether a turn is working.
inline avatar::Mood mood(const codex_hid::Slot& slot) {
  if (!slot.present) return avatar::Mood::Idle;
  switch (slot.color) {
    case 0x304ffe: return avatar::Mood::Thinking;
    case 0xff6d00: return avatar::Mood::NeedsInput;
    case 0xff0033: return avatar::Mood::Error;
    default: return avatar::Mood::Idle;
  }
}
inline uint16_t statusColor(const codex_hid::Slot& slot) {
  if (!slot.present || !slot.color) return monitor::kTrack;
  // LCD status is information, so a host lighting brightness of zero must not
  // erase it. An explicit black color still clears the slot's status.
  const float b = slot.brightness < 0.25f ? 0.25f : slot.brightness;
  return monitor::rgb(uint8_t((slot.color >> 16) * b),
      uint8_t(((slot.color >> 8) & 255) * b), uint8_t((slot.color & 255) * b));
}
}
