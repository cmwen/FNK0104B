# Monitor LED patterns — 2026-10-14

PlatformIO built the local monitor 0.8.3 and an isolated public image without the
ignored private bridge header. The local monitor upload verified all five image
hashes on FNK0104B B81F3FC39F94. The first upload attempt lost the USB serial
connection after the 1200-baud transition; PlatformIO rediscovered the same
board in bootloader mode and the retry succeeded with a watchdog reset.

A PlatformIO 115200-baud serial check through a temporary Windows COM9 loopback
relay confirmed version 0.8.3, Wi-Fi connected, both monitor workers, live
Desktop slot updates, 34,687 free internal heap bytes and 8,318,200 free PSRAM
bytes. Audio remained ready/muted with zero reported USB drops. The default
60-second notification expired while working updates continued. No panic or
RGB/RMT error was observed in this short capture.

The updated rgb-diag then built and uploaded with all four hashes verified.
It uses the monitor's frame renderer at 20% brightness and a two-second period.
Serial captured every phase 0–12: Solid/Blink/Breathe/Rainbow for working,
completion and attention, then off. This establishes execution of the renderer
and board writer for every combination. The owner reported not having checked the animation; visual appearance remains
UNKNOWN and is not inferred from phase logs.

The Windows physical BLE helper could not load its Windows.Devices metadata
assembly, before opening a BLE connection or writing settings. No BLE pattern
write or persistence is claimed; real browser BLE saving/readback, reboot
persistence and integrated completion/attention triggers remain UNKNOWN.

The final monitor restoration verified all five hashes. Post-restoration
serial confirmed version 0.8.3, live working updates, connected Wi-Fi, both
workers, 34,691 free internal heap bytes and zero USB audio drops. The 60-second
LED shutoff appeared while repeated working status continued. The monitor is
restored after the diagnostic. Both use the existing monitor
partition layout/NVS boundaries. No full-chip erase or irreversible security
setting was changed. Normal USB ownership returns to Windows.

Automated checks: 40 native tests, 44 host tests, 22 browser-client tests, four
guide tests, responsive browser checks and interactive preview checks passed.
The public build omits private bridge configuration; release packaging verifies
image hashes, version/layout/model compatibility and absence of the private
bridge token without printing it.

Logs: [initial upload](led-patterns-2026-10-14/initial-upload.log),
[initial runtime](led-patterns-2026-10-14/initial-runtime.log),
[diagnostic upload](led-patterns-2026-10-14/diag-upload.log),
[diagnostic runtime](led-patterns-2026-10-14/diag-runtime.log),
[BLE helper limitation](led-patterns-2026-10-14/ble-helper.log).

Final logs: [restoration upload](led-patterns-2026-10-14/final-upload.log),
[restored monitor runtime](led-patterns-2026-10-14/final-runtime.log).

## Public release

[Codex monitor 0.8.3](https://github.com/cmwen/FNK0104B/releases/tag/codex-monitor-v0.8.3)
was published with tag `codex-monitor-v0.8.3` targeting
`c6067e2ce55d035b11c2150040d07a0becff64cf`. The feature milestone is `655a0c4`;
the following journal-reference commit has identical firmware inputs.

The public image came from an isolated source snapshot without the ignored
bridge configuration. Firmware input fingerprints match the tagged source.
The portable bundle verifier confirmed image hashes, version 0.8.3, existing
layout/model compatibility and provenance. Private bridge-token absence was
checked without printing its value. Public application SHA-256:
`b141bc93b90c1a444cc2d600aea57d1c36d4d97be66762062cd2d9e23a4712e0`.
Local/public partition-table and model hashes match.

Release assets are the full-install ZIP, app-only `firmware.bin`, `ota.json`
and `SHA256SUMS`. GitHub's four uploaded asset digests matched the local
packaged files. The standalone OTA demo's latest-release selection is unchanged.
The broader firmware matrix and guide publication were still pending at
release verification; local/public monitor builds and local tests had passed.
