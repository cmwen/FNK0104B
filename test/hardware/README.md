# Hardware integration test plan

Run each diagnostic on a real FNK0104B and record board revision, firmware commit, PlatformIO environment, USB port, test result, and serial log. Start with one capability at a time.

| Capability | Future check |
|---|---|
| NVS | Write a namespaced value, reboot, read it back, then erase only that test key. |
| Wi-Fi | Join a local test access point using untracked credentials; report association and IP without logging secrets. |
| Display | Run `pio run -e display -t upload`; verify dimensions, backlight, solid colors, and text. |
| Touch | Run `pio run -e touch -t upload`; tap the panel and verify orientation and screen edges in serial output. |
| Calculator | After both diagnostics, run `pio run -e calculator -t upload`; exercise digits, operations, clear, delete, sign toggle, and divide-by-zero recovery. |
| SD | Mount with the verified four-bit SDIO pins, report card capacity, and perform a temporary file write/read/remove. |
| MQTT | Connect to a test broker, publish and subscribe to a unique test topic, then disconnect cleanly. |
| OTA | After a documented OTA-capable partition layout exists, update from a local test server and verify boot/rollback behavior. |

Do not combine these into an automated hardware test runner yet. Preserve the serial log for each run.
