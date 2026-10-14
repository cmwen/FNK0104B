#pragma once
#include <cstdint>
namespace ui::led {
enum class State : uint8_t { Off, Complete, Working, Attention };
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
