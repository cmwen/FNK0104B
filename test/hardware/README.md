# Hardware integration test plan

Run each diagnostic on a real FNK0104B and record board revision, firmware commit, PlatformIO environment, USB port, test result, and serial log. Start with one capability at a time.

| Capability | Future check |
|---|---|
| NVS | Write a namespaced value, reboot, read it back, then erase only that test key. |
| Wi-Fi | Join a local test access point using untracked credentials; report association and IP without logging secrets. |
| Display | Initialize ILI9341 and verify dimensions, backlight, solid colors, and text. |
| Touch | Read press/release and coordinates over serial; verify orientation mapping and interrupt behavior. |
| SD | Mount with the verified four-bit SDIO pins, report card capacity, and perform a temporary file write/read/remove. |
| MQTT | Connect to a test broker, publish and subscribe to a unique test topic, then disconnect cleanly. |
| OTA | After a documented OTA-capable partition layout exists, update from a local test server and verify boot/rollback behavior. |

Do not combine these into an automated hardware test runner yet. Preserve the serial log for each run.
