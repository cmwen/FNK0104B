#pragma once
#include <codex_hid/protocol.hpp>

namespace codex_hid {
// Only fixed-size lighting fields, never arbitrary host payloads or thread text.
// Each entry is slot:present:RGB:brightness.
inline void describeStatus(const Status& status, char (&output)[192]) {
  size_t used = 0;
  output[0] = 0;
  for (unsigned i = 0; i < 6; ++i) {
    const auto& slot = status.slots[i];
    const int n = snprintf(output + used, sizeof(output) - used,
        "%s%u:%u:%06lx:%.2f", i ? "," : "", i, unsigned(slot.present),
        static_cast<unsigned long>(slot.color), static_cast<double>(slot.brightness));
    if (n < 0 || size_t(n) >= sizeof(output) - used) return;
    used += size_t(n);
  }
}
}
