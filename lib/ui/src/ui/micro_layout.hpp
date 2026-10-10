#pragma once
#include <cstdint>
#include <ui/monitor_theme.hpp>

namespace ui::micro {
constexpr int tileX(unsigned slot) { return 6 + (slot % 3) * 104; }
constexpr int tileY(unsigned slot) { return 59 + (slot / 3) * 64; }
constexpr int tileAt(int x, int y) {
  for (unsigned slot = 0; slot < 6; ++slot)
    if (x >= tileX(slot) && x < tileX(slot) + 100 &&
        y >= tileY(slot) && y < tileY(slot) + 60) return slot;
  return -1;
}
// Internal input IDs 13..16 encode radial joystick events, not ACT keys.
constexpr const char* directionNames[] = {"Up", "Down", "Right", "Left"};
constexpr int directionX(unsigned n) { return 6 + n * 78; }
constexpr int directionAt(int x, int y) {
  for (unsigned n = 0; n < 4; ++n)
    if (x >= directionX(n) && x < directionX(n) + 74 && y >= 123 && y < 183)
      return 13 + n;
  return -1;
}
template<class Display> void directions(Display& d) {
  for (unsigned n = 0; n < 4; ++n) {
    const int x = directionX(n);
    monitor::frame(d, x, 123, 74, 60, monitor::kBorder);
    d.setTextColor(monitor::kText, monitor::kPanel);
    d.drawString(directionNames[n], x + 9, 136, 2);
    d.setTextColor(monitor::kMuted, monitor::kPanel);
    d.drawString("Joystick", x + 9, 163, 1);
  }
}
constexpr uint8_t commandKeys[] = {6, 7, 8, 9, 10, 12};
constexpr const char* commandNames[] = {"Fast", "Approve", "Reject", "Fork", "Mic", "Send"};
constexpr int bottomAt(int x, int y) {
  if (y < 191 || y >= 237) return -1;
  if (x >= 4 && x < 116) return 10;
  if (x >= 122 && x < 204) return 11; // The separate microphone switch.
  if (x >= 210 && x < 316) return 12;
  return -1;
}
template<class Display>
void tile(Display& d, unsigned slot, uint16_t color, bool known, bool selected,
          bool commands = false, bool avatars = false) {
  const int x = tileX(slot), y = tileY(slot);
  monitor::frame(d, x, y, 100, 60, selected ? monitor::kCyan : monitor::kBorder);
  d.fillRect(x + 7, y + 8, 5, 44, color ? color : monitor::kTrack);
  if (avatars && !commands) return;
  char label[16]; snprintf(label, sizeof(label), "Agent %u", slot + 1);
  d.setTextColor(monitor::kText, monitor::kPanel);
  d.drawString(commands ? commandNames[slot] : label, x + 18, y + 13, 2);
  d.setTextColor(monitor::kMuted, monitor::kPanel);
  d.drawString(commands ? ((slot == 1 || slot == 2) ? "Current req." : "Desktop key") : (known ? "Desktop slot" : "No status"), x + 18, y + 40, 1);
}
template<class Display>
void voice(Display& d, bool usb, bool ready, bool held, bool streaming, int level,
           bool recording = false, bool preparing = false, bool busy = false, bool muted = true,
           bool separateVoice = false, bool voiceOpen = false) {
  monitor::frame(d, 4, 191, usb ? 112 : 200, 46, held || recording ? monitor::kRed : monitor::kBorder);
  monitor::frame(d, 210, 191, 106, 46, monitor::kBorder);
  d.setTextColor(monitor::kText, monitor::kPanel);
  d.drawString(usb ? "Hold to talk" : "Orchestrator", 12, 199, 2);
  d.drawString("Send", 224, 199, 2);
  d.setTextColor((usb ? !muted && ready && streaming : ready) ? monitor::kMint : monitor::kMuted, monitor::kPanel);
  const char* audioState = muted ? "Mic OFF" : !ready ? "No input" : !streaming ? "Waiting" : "Mic ON";
  d.drawString(usb ? (voiceOpen ? "Voice audio" : audioState) : !ready ? "Mic unavailable" :
    recording ? "Recording / tap to stop" : preparing ? "Preparing..." : busy ? "Sending..." : "Tap / Hi ESP", 12, 222, 1);
  d.drawString("Desktop key", 224, 222, 1);
  if (usb) {
    monitor::frame(d, 122, 191, 82, 46, voiceOpen ? monitor::kMint : monitor::kBorder);
    d.setTextColor(separateVoice ? monitor::kText : monitor::kMuted, monitor::kPanel);
    d.drawString("Voice", 130, 199, 2);
    d.setTextColor(voiceOpen ? monitor::kMint : monitor::kMuted, monitor::kPanel);
    d.drawString(!separateVoice ? "Setup" : voiceOpen ? audioState : "Mic OFF", 130, 222, 1);
  }
  const int heights[] = {4,9,16,25,16,9,4};
  for (int i = 0; i < 7; ++i) {
    const int h = (usb ? streaming && !muted : recording) ? 2 + heights[i] * level / (usb ? 250 : 100) : 2;
    d.drawFastVLine((usb ? 86 : 170) + i * 4, (usb ? 228 : 213) - h / 2, h, monitor::kCyan);
  }
}
}
