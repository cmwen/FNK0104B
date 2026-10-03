#pragma once
#include <cstddef>
#include <cstdint>

namespace speech {
constexpr uint32_t kCommandWindowMs = 12000;
// Command IDs are one-based indices into this same list used by the UI.
constexpr const char* kMonitorCommands[] = {
    "start listening", "go back", "show status",
    "turn on the screen", "turn off the screen"};
constexpr size_t kMonitorCommandCount = sizeof(kMonitorCommands) / sizeof(kMonitorCommands[0]);
}
