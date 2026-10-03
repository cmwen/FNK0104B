#pragma once
#include <stdint.h>

namespace speech_screen {
enum class Phase { Starting, Wake, Listening, Recognized, Timeout, Error };
struct Snapshot {
  Phase phase = Phase::Starting;
  unsigned wakes = 0, commands = 0, timeouts = 0;
  int32_t peak = 0;
  int64_t deadline_us = 0;
  float probability = 0;
  bool vad_ready = false;
  bool speech = false;
  char last_command[48] = {};
  char error[64] = {};
};
bool begin();
void publish(const Snapshot& snapshot);
}
