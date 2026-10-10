#pragma once
#include <BLEServer.h>
namespace monitor_ota {
constexpr char version[] = "0.7.1";
constexpr char layout[] = "monitor-ota4m-model810000-v1";
void attach(BLEService* service);
void begin(bool coreReady);
void tick();
bool requested();
bool busy();
// Main loop checks voice activity and quiesces audio before allowing work.
void start(bool allowed, TaskHandle_t workerTask);
// Run on the existing voice worker: flash writes need an internal-RAM stack.
bool runPending();
void request(const char* command);
}  // namespace monitor_ota
