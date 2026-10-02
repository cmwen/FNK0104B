#pragma once

#include <stdint.h>

namespace ui {
class IdleTimer {
 public:
  void activity(uint32_t now) { lastActivity_ = now; }
  bool expired(uint32_t now, uint32_t timeoutMs, bool busy) {
    if (busy) activity(now);
    return !busy && static_cast<uint32_t>(now - lastActivity_) >= timeoutMs;
  }

 private:
  uint32_t lastActivity_ = 0;
};
}  // namespace ui
