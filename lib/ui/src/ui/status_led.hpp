#pragma once
#include <cstdint>
namespace ui::led {
enum class State : uint8_t { Off, Complete, Working, Attention };
enum class Pattern : uint8_t { Solid, Blink, Breathe, Rainbow };
struct Color {
  uint8_t red = 0, green = 0, blue = 0;
  bool operator==(const Color& other) const {
    return red == other.red && green == other.green && blue == other.blue;
  }
};
// elapsed is relative to the notification, not the latest status heartbeat.
inline Color frame(State state, Pattern pattern, uint8_t brightness,
                   uint16_t periodMs, uint32_t elapsed) {
  if (state == State::Off || !brightness || !periodMs) return {};
  uint32_t level = (uint32_t(brightness) * 255 + 50) / 100;
  const uint32_t phase = elapsed % periodMs;
  if (pattern == Pattern::Blink && phase >= periodMs / 2) return {};
  if (pattern == Pattern::Breathe) {
    const uint32_t triangle = phase * 512 / periodMs;
    const uint32_t t = triangle <= 256 ? triangle : 512 - triangle;
    // Smoothstep eases into the dark and bright ends of each cycle.
    const uint32_t eased = t * t * (768 - 2 * t) / 65536;
    level = level * eased / 256;
  }
  if (pattern == Pattern::Rainbow) {
    const uint32_t wheel = phase * 1536 / periodMs;
    const uint8_t rising = level * (wheel % 256) / 255;
    const uint8_t falling = level - rising;
    const uint8_t peak = level;
    switch (wheel / 256) {
      case 0: return {peak, rising, 0};
      case 1: return {falling, peak, 0};
      case 2: return {0, peak, rising};
      case 3: return {0, falling, peak};
      case 4: return {rising, 0, peak};
      default: return {peak, 0, falling};
    }
  }
  return {uint8_t(state == State::Attention || state == State::Complete ? level : 0),
          uint8_t(state == State::Working || state == State::Complete ? level : 0), 0};
}
// One bounded notification per meaningful state/identity change. Heartbeats,
// effect animation and brightness changes must not extend its lifetime.
class Indicator {
 public:
  State update(State state, uint32_t identity, bool connected, bool enabled,
               uint16_t seconds, uint32_t now) {
    if (!connected) { reset(); return State::Off; }
    if (state != previous_ || identity != identity_) {
      const bool completed = state == State::Off &&
          (previous_ == State::Working || previous_ == State::Attention);
      shown_ = completed ? State::Complete : state;
      started_ = now;
      previous_ = state; identity_ = identity;
    }
    if (!enabled || uint32_t(now - started_) >= uint32_t(seconds) * 1000)
      shown_ = State::Off;
    return shown_;
  }
  void reset() { previous_ = shown_ = State::Off; identity_ = 0; }
 private:
  State previous_ = State::Off, shown_ = State::Off;
  uint32_t identity_ = 0, started_ = 0;
};
}
