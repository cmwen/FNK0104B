#pragma once
namespace monitor_wifi_setup {
// Prepare the provisioning code before microphone or radio initialization.
void prepare();
// Call before normal BLE initialization. Setup owns the radios until reboot.
bool begin(bool requested, bool skip);
void loop();
void request();
// Automatic first-boot provisioning yields to a discovered USB Micro session.
void yieldToMicro();
}
