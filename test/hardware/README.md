# Hardware integration test plan

Run each diagnostic on a real FNK0104B and record board revision, firmware commit, PlatformIO environment, USB port, test result, and serial log. Start with one capability at a time.

| Capability | Future check |
|---|---|
| NVS | Write a namespaced value, reboot, read it back, then erase only that test key. |
| Wi-Fi | Join a local test access point using untracked credentials; report association and IP without logging secrets. |
| Display | Run `pio run -e display -t upload`; verify dimensions, backlight, solid colors, and text. |
| Avatar animation | After display passes, run `pio run -e avatar-diag -t upload`; check four distinct upright portraits, all four animated states and badges, matching red/green/blue chip pairs, and absence of flicker or trails. Read the 115200-baud timing report. |
| Touch | Run `pio run -e touch -t upload`; tap the panel and verify orientation and screen edges in serial output. |
| Screen timeout | After display and touch checks pass, run `pio run -e screen-timeout -t upload`; verify the backlight turns off after 60 seconds and returns on touch. |
| Calculator | After both diagnostics, run `pio run -e calculator -t upload`; exercise digits, operations, clear, delete, sign toggle, and divide-by-zero recovery. |
| SD | Mount with the verified four-bit SDIO pins, report card capacity, and perform a temporary file write/read/remove. |
| MQTT | Connect to a test broker, publish and subscribe to a unique test topic, then disconnect cleanly. |
| OTA | With the existing 3 MiB dual-slot layout, install `ota` over USB, publish a newer GitHub Release with an `ota.bin` asset, confirm the offered version with `y` in the serial monitor, and verify the new version boots. Test rollback with a deliberately non-booting image only after arranging a recovery path. |

Do not combine these into an automated hardware test runner yet. Preserve the serial log for each run.
