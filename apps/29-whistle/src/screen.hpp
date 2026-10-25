#pragma once
#include <cstdint>
namespace whistle_screen {
enum class Phase { Loading, Ready, Recording, Processing, Result, Error };
struct Snapshot {
  Phase phase = Phase::Loading;
  unsigned milliseconds = 0;
  int peak = 0;
  char language[8] = {};
  char text[512] = "Loading Whistle model";
};
bool begin();
void enableTouch();
void publish(const Snapshot& snapshot);
bool takeRecordToggle();
}
