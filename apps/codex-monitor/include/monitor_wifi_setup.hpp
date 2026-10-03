#pragma once
namespace monitor_wifi_setup {
// Call before normal BLE initialization. Setup owns the radios until reboot.
bool begin(bool requested, bool skip);
void loop();
void request();
}
