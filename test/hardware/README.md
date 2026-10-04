# Hardware integration checks

Record the board revision, firmware commit, PlatformIO environment, USB port, result, and 115200-baud serial log for each new run. This table distinguishes results already captured in the repository from checks still needed; see [monitor readiness](../../docs/monitor-readiness.md) for the dashboard decision.

| Capability | Recorded evidence | Next useful check |
|---|---|---|
| Hello / memory | Upload hash verification, serial heartbeats, and partial PSRAM output. | Capture the complete boot banner, including flash size, after reset. |
| Display / avatars | Photo confirmed `INVON` colors and avatar layout; timing and memory values recorded. | Recheck after a future display-driver change. |
| Microphone | Spoken peaks above 1000 captured with GPIO6 input. | Recheck after an audio-driver change. |
| OTA | USB-installed 0.1.0 updated over HTTPS to 0.2.0 and accepted at boot. | Test the touch controls added later; arrange recovery before any rollback experiment. |
| Touch | App implemented; no saved edge/orientation result. | Tap corners/edges in `touch`, then test combined UI targets. |
| USB HID keyboard | [HID diagnostic and keyboard run](hid-keyboard-2026-10-04.md): builds and uploads verified, HID plus CDC enumeration, ready-state serial evidence, and owner-confirmed touchscreen input directly on Windows 11. | Check individual keys/Num Lock, emoji selection and sustained use; the owner's Windows App remote-session input path did not work as expected. |
| Wi-Fi / BLE provisioning | OTA confirms saved Wi-Fi connection; setup paths have no saved end-to-end result. | Provision a test network and confirm reconnect after reboot without logging its password. |
| LVGL combined screen | Apps implemented; no saved sustained run. | Exercise touch and Wi-Fi updates while watching heap, redraw, and responsiveness. |
| Speaker | Piano diagnostic implemented; no saved audible result. | Connect a speaker and check notes, release-to-silence, and volume control. |
| SD / file manager | Apps implemented; no saved card run. | Mount a FAT card, inspect capacity and files; test any write operation separately with disposable data. |
| Screen timeout / calculator | Apps implemented; no saved result. | Check wake-on-touch and calculator input on the panel if these behaviors will be reused. |
| NVS / MQTT | Buildable placeholders. | Add small standalone diagnostics only when their behavior is needed. |
| Codex monitor | [Mock board run](codex-monitor-mock-2026-09-29.md): build and USB upload passed; serial confirmed quota, agent, attention, completion, offline, and recovery transitions; owner confirmed stable landscape view. | Check touch targets, actual voice upload, BLE settings, audible tone, and live Codex bridge. |

Do not combine these into an automated hardware test runner yet. Preserve a serial log and the outcome for every new device check.

[Monitor restoration after HID testing](codex-monitor-restored-after-hid-2026-10-04.md)
records the successful PlatformIO build/upload, 115200-baud wake-state audio,
and LCD readback showing Wi-Fi Online and live Codex agents on 2026-10-04.
