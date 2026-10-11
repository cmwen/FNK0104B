# USB avatar status and independent layout — 2026-10-11

PlatformIO Core CLI rebuilt and uploaded the current local monitor firmware to
FNK0104B B81F3FC39F94 through `/dev/ttyACM0`. Application SHA-256:
`87817d52bb4889394164fd211eda9760ae2a2f4a5f14b2b42d43aefe86c56139`.
The final build identifies as monitor **0.7.2**. The earlier local 0.7.1 build
also received the status and layout checks below before diagnostic commands
and the release version were added.

All five uploaded image hashes were verified. The existing bootloader, partition
table, OTA initializer, models and application were written at their configured
addresses. NVS at `0x9000`/`0x5000` and partition boundaries were preserved. No
full-chip erase, eFuse or security-setting change was performed. See
[final upload log](usb-avatar-status-2026-10-11/upload-0.7.2.log).

Windows initially owned the board's HID/audio/COM9 interfaces. The board entered
ROM USB Serial/JTAG mode and was attached to WSL for upload. After watchdog reset,
it returned to Windows as 303a:8360 with COM9/HID/TinyUSB UAC1. A temporary localhost
COM9 relay let PlatformIO monitor at 115200 while Desktop retained HID/audio.
The relay is a diagnostic aid, not a firmware dependency.

## Actual LCD and live status

The saved three-slot avatar preference survived the upload. Actual LCD readback
confirmed three portraits blended into the panel background, the four direction
shortcuts, status strips, selection border, and bottom voice controls.

- [Initial LCD](usb-avatar-status-2026-10-11/initial-layout.png) showed slot 0
  white/idle, slot 1 blue/thinking and slot 2 white/idle.
- [Later LCD](usb-avatar-status-2026-10-11/refreshed-layout.png) showed slots 0 and
  1 blue/thinking and slot 2 white/idle. Avatar identities stayed consistent.
  Status strips remained visible through the captured refreshes.
- Live `v.oai.thstatus` packets were decoded, acknowledged and delivered to the
  display. No synthetic HID status or key events were injected into Desktop.
- Serial health reported saved Wi-Fi connected (`wifi=3`), both workers present,
  internal heap 41,279 bytes and PSRAM approximately 8,318,564 bytes. USB input
  stayed ready, outgoing microphone audio remained muted, and `usb_dropped=0`.
  No panic or disconnect/reset was observed in the captured runtime.

See [runtime diagnostics](usb-avatar-status-2026-10-11/runtime.log).

## Six-avatar LCD check

The final 0.7.2 build reported `slots=3 avatars=1`. The temporary serial command
`micro-six` changed only the active layout, without writing NVS preferences or
changing appearance. Health then reported `slots=6 avatars=1`, and an
[actual LCD capture](usb-avatar-status-2026-10-11/six-avatars-0.7.2.png) showed
six portraits in two rows, live blue/green/white status strips, and the bottom
voice controls. The first blue slot displayed its thinking expression. The
`micro-three` command restored the original active layout after capture.

## Limits

The Windows-native BLE diagnostic could not discover/read settings (Windows
API limitations and a discovery timeout); it made no preference writes. Physical
BLE saving, touch selection/shortcuts, orange/red mood transitions, continuous
animation smoothness and sustained dropout recovery remain **UNKNOWN**.
The owner reported saving six-agent avatar settings and reading six slots back
after reconnecting. However, a follow-up
[actual LCD capture](usb-avatar-status-2026-10-11/after-settings-attempt.png)
still showed three avatars, and the subsequent firmware health readout showed
three slots. A successful physical BLE write is therefore not established by
these observations. The final firmware logs accepted settings writes to help
resolve the discrepancy. Host previews and 14 browser tests cover
layout/appearance independence; those do not establish physical BLE acceptance.
