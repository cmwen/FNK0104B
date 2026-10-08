# Wireless settings / compact USB Micro — 2026-10-08

## Software checks

- PlatformIO `codex-monitor` final build passed; firmware version 0.6.1.
- Firmware SHA256: `a22cfb7600dd919e8119db124b5ab8d8b6f4c486e9607ede4de78c6761409057`.
- PlatformIO native: 26 tests passed. Browser BLE client: six tests passed.
- Astro guide built 42 pages; both guide checks passed after the build.
- Shared C++ renderer produced the compact 320×240 preview below. It uses
  sample data; it is not LCD readback or a physical touch test.

![Host preview](../../docs/images/usb-micro-compact-preview.png)

## Upload

Sandbox discovery returned no board; discovery outside the sandbox found the
known FNK0104B (B81F3FC39F94) at `/dev/ttyACM0`, USB 303a:8360.
The initial PlatformIO upload entered ROM mode but timed out during enumeration.
ROM subsequently appeared as 303a:1001. A temporary PlatformIO configuration
skipped only 1200-baud entry and port wait. After the final compatibility change,
the final upload used `--disable-auto-clean` with that recovery configuration.
It completed and verified all image hashes. See [upload evidence](usb-micro-controls-2026-10-08/upload.log).
The watchdog reset returned the board to application USB 303a:8360 on Windows,
with HID, CDC (COM9) and TinyUSB UAC1 interfaces.

The existing bootloader, partition table, speech models and application were
written at their existing offsets. No partition boundaries, GPIOs, stored
credentials, eFuses or security settings were changed; no erase-all was used.
The first successful upload preceded the final backward-compatibility adjustment;
only the final upload log/hash above describe the delivered build.

## Runtime at 115200 baud

A temporary localhost-only Windows COM9 forwarder allowed **PlatformIO Core
serial monitoring at 115200 baud** while Windows owned HID/audio. It closed
after the check; no permanent service was installed and USB remains with Windows.
See [runtime evidence](usb-micro-controls-2026-10-08/runtime.log).

- `monitor_audio ready=1 reason=none usb_stream=0 usb_dropped=0`.
- `monitor_health heap=44867 psram=8318700 wifi=3 status_task=1 voice_task=1`.
- Local speech remained dormant (`speech=0 speech_reason=starting`) during
  USB Micro use, consistent with the current USB audio policy.
- Incoming `v.oai.rgbcfg`, `v.oai.thstatus` and `device.status` were decoded;
  report-6 replies completed and status revisions 4/5 reached the application.
- No panic, reboot or microphone error was observed during the 25-second capture.
  This is a short health check, not sustained stability or USB audio capture proof.

The BLE advertisement check could not run through Windows PowerShell's WinRT
interoperability. Neither that tooling failure nor successful USB status traffic
establishes browser BLE recovery. BLE discovery/read/write/reconnect, saved-layout
persistence, protected Wi-Fi provisioning, physical joystick touch/neutral return,
Desktop direction assignments and perceived LCD refresh remain **UNKNOWN**.
No synthetic input was sent to live Desktop sessions during this check.

The [decision and setup guide](../../docs/usb-micro-controls.md) describes the
code-supported BLE conflict, unchanged-packet redraw fix, stable slot policy,
settings compatibility and remaining acceptance checks.
