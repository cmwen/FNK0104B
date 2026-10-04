#include <Arduino.h>
#include <fnk0104b/board.hpp>
#include <fnk0104b/keyboard.hpp>
#include <keypad/layout.hpp>

namespace {
auto& tft = fnk0104b::display.driver();
auto& hid = fnk0104b::keyboard;
keypad::Host host = keypad::Host::Windows;
bool emoji_page = false, was_pressed = false, touch_ready = false;
bool connected = false, num_lock = false;
constexpr uint16_t bg = 0x0843, tile = 0x1927, accent = 0x05d7;
const char* message = "Focus a text field on your computer";
const char* hostName() {
  return host == keypad::Host::Windows ? "Windows" : host == keypad::Host::Mac ? "Mac" : "Linux";
}
void button(int x, int y, int w, int h, const char* label, uint16_t color = tile) {
  tft.fillRoundRect(x, y, w, h, 7, color);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, color);
  tft.drawString(label, x + w / 2, y + h / 2, 2);
}
void draw() {
  tft.fillScreen(bg);
  button(8, 5, 97, 29, "Numpad", emoji_page ? tile : accent);
  button(111, 5, 97, 29, "Emoji", emoji_page ? accent : tile);
  button(214, 5, 98, 29, hostName());
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(connected ? TFT_GREEN : TFT_ORANGE, bg);
  tft.drawString(!touch_ready ? "Touch unavailable" : !connected ? "Connect USB to a host" :
                 num_lock ? "USB ready / Num Lock ON" : "USB ready / Num Lock OFF", 8, 36, 2);
  if (!emoji_page) {
    for (int i = 0; i < 20; ++i)
      button(8 + i % 4 * 76, 58 + i / 4 * 31, 70, 26, keypad::numbers[i].label);
  } else {
    for (int i = 0; i < 12; ++i) {
      const int x = 8 + i % 4 * 76, y = 58 + i / 4 * 39;
      button(x, y, 70, 34, keypad::emojis[i].label);
    }
    button(8, 177, 146, 31, host == keypad::Host::Linux ? "Unicode help" : "Open picker");
    button(160, 177, 70, 31, "Enter");
    button(236, 177, 76, 31, "Esc");
  }
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TFT_LIGHTGREY, bg);
  tft.drawString(message, 8, 219, 1);
}
bool picker() {
  // USB modifiers: Ctrl=1, Shift=2, GUI/Command=8.
  if (host == keypad::Host::Windows) return hid.tap(0x37, 0x08); // Win + period
  if (host == keypad::Host::Mac) return hid.tap(0x2c, 0x09); // Ctrl + Command + Space
  return false; // Linux desktops have no universal picker shortcut.
}
bool sendEmoji(int index) {
  const auto& e = keypad::emojis[index];
  if (host == keypad::Host::Linux) {
    char hex[9];
    snprintf(hex, sizeof(hex), "%lx", static_cast<unsigned long>(e.codepoint));
    if (!hid.tap(0x18, 0x03) || !hid.text(hex) || !hid.tap(0x28)) return false;
    // Heart needs emoji presentation rather than the text heart glyph.
    if (e.codepoint == 0x2764)
      return hid.tap(0x18, 0x03) && hid.text("fe0f") && hid.tap(0x28);
    return true;
  }
  // macOS search focus differs by viewer state: open the picker for manual choice.
  if (!picker()) return false;
  if (host == keypad::Host::Mac) return true;
  delay(650); // Windows picker animation/focus; timing requires host validation.
  return hid.text(e.search); // User selects the result; never guesses an emoji.
}
void touch(int x, int y) {
  Serial.printf("keyboard_touch=x:%d y:%d usb_connected:%d endpoint_ready:%d\n", x, y, hid.ready(), hid.endpointReady());
  if (y >= 5 && y < 34) {
    if (x >= 8 && x < 105) emoji_page = false;
    else if (x >= 111 && x < 208) emoji_page = true;
    else if (x >= 214 && x < 312)
      host = static_cast<keypad::Host>((static_cast<unsigned>(host) + 1) % 3);
    message = emoji_page ? (host == keypad::Host::Linux ? "Linux: GTK Unicode input required" :
              host == keypad::Host::Mac ? "Mac: choose emoji in Character Viewer" : "Tap favorite, choose result on PC") : "Num toggles digits / navigation";
    draw();
    return;
  }
  if (!hid.ready()) { message = "USB keyboard is not ready"; draw(); return; }
  bool ok = false;
  if (!emoji_page) {
    const int i = keypad::cell(x, y, 58, 31, 5);
    if (i < 0) return;
    ok = hid.tap(keypad::numbers[i].usage);
    message = ok ? "Key sent" : "USB send failed";
  } else {
    const int i = keypad::cell(x, y, 58, 39, 3);
    if (i >= 0) {
      ok = sendEmoji(i);
      message = !ok ? "USB send failed" : host == keypad::Host::Linux ? "Unicode sent (compatible apps only)" : "Choose emoji on host, then Esc";
    } else if (y >= 177 && y < 208) {
      if (x >= 8 && x < 154) {
        if (host == keypad::Host::Linux) { message = "Use favorites in GTK text fields"; draw(); return; }
        ok = picker();
      } else if (x >= 160 && x < 230) ok = hid.tap(0x28);
      else if (x >= 236 && x < 312) ok = hid.tap(0x29);
      else return;
      message = ok ? "Shortcut sent" : "USB send failed";
    } else return;
  }
  Serial.printf("keyboard_action=%s host=%s\n", ok ? "sent" : "failed", hostName());
  draw();
}
}
void setup() {
  hid.begin();
  fnk0104b::board.begin();
  fnk0104b::board.printStartupInfo("keyboard", "0.1.1");
  Serial.println("Send t over serial for a deliberate 123 test in the focused host app.");
  fnk0104b::display.begin(1);
  touch_ready = fnk0104b::touch.begin();
  connected = hid.ready();
  num_lock = hid.numLock();
  draw();
}
void loop() {
  fnk0104b::TouchPoint p = {};
  if (touch_ready && fnk0104b::touch.read(p)) {
    if (p.pressed && !was_pressed) touch(p.x, p.y);
    was_pressed = p.pressed;
  } // A read failure preserves the press latch to avoid duplicate macros.
  const bool now_ready = hid.ready(), now_num = hid.numLock();
  if (now_ready != connected || now_num != num_lock) {
    connected = now_ready;
    num_lock = now_num;
    draw();
  }
  static uint32_t last = 0;
  if (millis() - last >= 3000) {
    last = millis();
    Serial.printf("hid_ready=%d endpoint_ready=%d num_lock=%d touch_ready=%d host=%s\n", connected, hid.endpointReady(), num_lock, touch_ready, hostName());
  }
  if (Serial.available() && Serial.read() == 't') {
    const bool ok = hid.text("123");
    Serial.printf("keyboard_serial_test=%s\n", ok ? "sent" : "failed");
  }
  delay(5);
}
