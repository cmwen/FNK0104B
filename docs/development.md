# Development toolchain

This repository uses PlatformIO Core CLI. No Arduino IDE is required.

## Pinned stack

| Layer | Version / policy | Reason |
|---|---|---|
| PlatformIO Core | 6.2.0 locally and in CI | Current release checked on 2026-09-27; install in a user environment, without `sudo`. |
| Espressif32 platform | `platformio/espressif32@7.0.1` in `platformio.ini` | Current official stable platform release checked on 2026-09-27. |
| Arduino-ESP32 framework | 2.0.17, selected by the official PlatformIO platform | The platform still packages this version. Moving to Arduino 3 needs a separate compatibility migration. |
| Native platform | `platformio/native@1.2.1` | Current official native release checked on 2026-09-27. |
| Host Python | 3.14 in CI | Explicit CI interpreter; PlatformIO Core manages build packages. |
| WSL Python | 3.14.7 | Matches Python.org's current Python 3 release checked on 2026-09-27. |
| WSL Git | 2.55.0 | Matches git-scm.com's current source release checked on 2026-09-27. |
| Codex CLI | 0.157.1 on this host | Matches the latest entry in the official Codex changelog checked on 2026-09-27; optional for building firmware. |

Install PlatformIO Core using the [official CLI installation guide](https://docs.platformio.org/en/latest/core/installation/methods/installer-script.html) or `python3 -m pip install --user 'platformio==6.2.0'` in an environment that permits user installs. Confirm with `pio --version`. The WSL installation already has Core 6.2.0; do not install a second copy merely to run this repository.

Build with `pio run` or `pio run -e <app>`. Run `pio test -e native` for host tests. The GitHub Actions workflow builds each named firmware target and runs native tests; it never uploads to a board. The platform pin fixes the version used by local builds and CI. Review [PlatformIO's Espressif32 releases](https://github.com/platformio/platform-espressif32/releases) and rebuild before changing it.

The official PlatformIO 7.0.1 release includes ESP-IDF 6.0.1 as an *alternative framework*. These applications still use Arduino 2.0.17, which is based on ESP-IDF 4.4.7. Shared low-level code can be migrated deliberately when an ESP-IDF application is introduced.

## USB and debugging on this WSL host

USB serial upload and 115200-baud monitoring worked on 2026-09-27 when `usbipd` attached the board and `dialout` group membership was active. USB detached during some resets; Windows `usbipd attach --wsl --busid 3-1 --auto-attach` reattached it. The PowerShell auto-attach loop must remain running for that behavior. A later upload with platform 7.0.1 verified successfully. The user subsequently captured `status=running`, followed by an `Input/output error` when USB/IP disconnected. The board reappeared as `/dev/ttyACM1`; see `docs/flashing.md` for its persistent `/dev/serial/by-id/` path. Monitoring that link then received four hello heartbeats over roughly 30 seconds with no disconnect. A complete startup banner is still pending.

Source-level JTAG debugging is **not yet verified**. The earlier `pio debug -e hello --interface=gdb` attempt failed because PlatformIO's bundled Xtensa GDB requires `libpython2.7.so.1.0`, absent on this Ubuntu installation. The `hello-debug` environment builds the same hello app with symbols and selects the standalone Espressif GDB declared by the current PlatformIO platform. That GDB starts on this host without Python 2.7. Use `pio debug -e hello-debug --interface=gdb` after USB permissions are configured. Its `debug_load_mode = manual` avoids an automatic firmware write during debugger launch.

A direct OpenOCD attempt could not open the USB JTAG interface (`LIBUSB_ERROR_ACCESS`); serial `dialout` permission does not grant JTAG USB access. [Espressif's ESP32-S3 JTAG guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-guides/jtag-debugging/configure-builtin-jtag.html) requires Linux udev rules for OpenOCD. The repository provides `scripts/99-fnk0104b-jtag.rules`, scoped to the board's observed USB ID `303a:1001` and Ubuntu's `plugdev` group. `cmwen` is already in that group. Install it once in WSL with administrator access:

```bash
sudo install -m 0644 scripts/99-fnk0104b-jtag.rules /etc/udev/rules.d/99-fnk0104b-jtag.rules
sudo udevadm control --reload-rules
sudo udevadm trigger --subsystem-match=usb
```

Then detach and reattach the board through `usbipd` (or unplug/replug it), verify the matching `/dev/bus/usb/` node is accessible to `plugdev`, and retry `pio debug -e hello-debug --interface=gdb`. Do not claim breakpoint debugging works until OpenOCD connects and GDB reaches the target. A newer Espressif32 platform pin alone does not change its Arduino 2.0.17 compiler toolchain or bundled OpenOCD version. See `docs/flashing.md` for the working serial path.

## Sources

- [PlatformIO Core releases on PyPI](https://pypi.org/project/platformio/)
- [PlatformIO Espressif32 7.0.1 release](https://github.com/platformio/platform-espressif32/releases/tag/v7.0.1)
- [PlatformIO Native releases](https://github.com/platformio/platform-native/releases)
- [Arduino-ESP32 releases](https://github.com/espressif/arduino-esp32/releases)
- [PlatformIO GitHub Actions guide](https://docs.platformio.org/en/latest/integration/ci/github-actions.html)
- [Official Codex changelog](https://learn.chatgpt.com/docs/changelog)
- [Python source releases](https://www.python.org/getit/source/)
- [Git releases](https://git-scm.com/install/)
