#pragma once
#include <stdint.h>
#include <stddef.h>

namespace keypad {
enum class Host : uint8_t { Windows, Mac, Linux };
struct Key { const char* label; uint8_t usage; };
// USB HID Keyboard/Keypad usages, not Arduino's ASCII/keycode translation.
constexpr Key numbers[] = {
    {"Num", 0x53}, {"/", 0x54}, {"*", 0x55}, {"BS", 0x2a},
    {"7", 0x5f}, {"8", 0x60}, {"9", 0x61}, {"-", 0x56},
    {"4", 0x5c}, {"5", 0x5d}, {"6", 0x5e}, {"+", 0x57},
    {"1", 0x59}, {"2", 0x5a}, {"3", 0x5b}, {"Enter", 0x58},
    {"0", 0x62}, {".", 0x63}, {"Tab", 0x2b}, {"Esc", 0x29}};
struct Emoji { const char* label; const char* search; uint32_t codepoint; };
constexpr Emoji emojis[] = {
    {"Smile", "smile", 0x1f604}, {"Laugh", "joy", 0x1f602},
    {"Love", "heart", 0x2764}, {"Like", "thumbs up", 0x1f44d},
    {"Party", "party", 0x1f389}, {"Fire", "fire", 0x1f525},
    {"Thanks", "pray", 0x1f64f}, {"Think", "thinking", 0x1f914},
    {"Cool", "sunglasses", 0x1f60e}, {"Sad", "cry", 0x1f622},
    {"Check", "check", 0x2705}, {"Rocket", "rocket", 0x1f680}};
inline int cell(int16_t x, int16_t y, int top, int height, int rows) {
  if (x < 8 || x >= 312 || y < top || y >= top + rows * height) return -1;
  const int col = (x - 8) / 76, row = (y - top) / height;
  if ((x - 8) % 76 >= 70 || (y - top) % height >= height - 5) return -1;
  return row * 4 + col;
}
}  // namespace keypad
