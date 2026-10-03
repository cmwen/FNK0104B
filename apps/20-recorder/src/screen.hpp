#pragma once
#include <cstdint>
namespace recorder_screen {
enum class Phase { Loading, Ready, Recording, Saving, Playing, Error };
enum class Action { Record, Play, Previous, Next, Select };
struct Event { Action action; unsigned row = 0; int64_t sampled_us = 0; };
struct Snapshot {
  Phase phase = Phase::Loading;
  bool speech = false;
  int peak = 0;
  unsigned seconds = 0, count = 0, page = 0, selected_row = 0;
  char files[3][24] = {};
  char message[48] = "Starting recorder";
};
bool begin();
void enableTouch();
void publish(const Snapshot& snapshot);
bool poll(Event& event);
}
