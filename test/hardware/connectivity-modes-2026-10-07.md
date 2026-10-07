# Connectivity modes device check — 2026-10-07

User authorized flashing and testing the attached FNK0104B.

## Build and flash

- The board was present outside the sandbox at `/dev/ttyACM0`, serial
  B81F3FC39F94, USB 303a:8360. Sandboxed discovery incorrectly showed no device.
- `pio run -e codex-monitor` passed before upload, using the current repository
  configuration: Espressif platform 7.0.1, IDF 5.5.5 and Xtensa toolchain 14.2.0.
- Firmware SHA256:
  `1b076eaf16522223b44f27c8dba4ee8b63f771bee9e7f555663f13caf3adb414`.
  Static RAM: 151,836 bytes; flash: 3,119,249 bytes.
- The first upload entered ROM mode but timed out waiting for WSL re-enumeration.
  The ROM port then appeared at `/dev/ttyACM0`, USB 303a:1001.
- A temporary copy of platformio.ini disabled only the 1200-baud touch and
  upload-port wait. PlatformIO upload succeeded, identified ESP32-S3 revision
  v0.2 / MAC B8:1F:3F:C3:9F:94, and verified all written image hashes.
- Writes were the bootloader at 0x0, existing partition table at 0x8000, models
  at 0x610000 (3,518,072 bytes), and app at 0x10000 (3,119,648 bytes). The watchdog
  reset started the app. No erase-all, new partition boundaries, NVS reset,
  GPIO assignment or security changes were performed.

The [upload log](connectivity-modes-2026-10-07/upload.log) retains the device
write/hash evidence.

## USB Micro and raw audio

Windows reclaimed the application interfaces after reboot. A temporary
localhost-only COM9 bridge allowed **PlatformIO Core serial monitoring at
115200 baud** while Windows owned HID/audio.

The [runtime log](connectivity-modes-2026-10-07/usb-micro-runtime.log) records:

- Healthy raw microphone: `ready=1 reason=none`; zero USB drops at the time
  of the logged reports. Wi-Fi connected; bridge status/voice workers present.
- `speech=0 speech_reason=starting`, with approximately 8.3 MB free PSRAM and
  no AFE/inference reports during this capture. This is consistent with the
  initial USB discovery suppressing local model startup, not a speech failure.
- Valid native host `v.oai.rgbcfg`, `v.oai.thstatus` and `device.status` requests,
  complete report-6 replies, and application status-display revisions. Host
  identity is not authenticated by this protocol.
- Bridge status continued updating while Micro owned local controls.

A Windows WinMM recording selected **Microphone (TinyUSB UAC1)** explicitly,
without changing the default input. It captured **160,000 bytes / 80,000
samples**, 16 kHz mono PCM16, over five seconds. Peak was 54, RMS approximately
11.13, with 77,125 nonzero samples. This proves delivery of actual raw USB
audio in this quiet capture; audible speech quality, Desktop transcription
and recognition accuracy were not tested. The WAV remains temporary; only
[measurement output](connectivity-modes-2026-10-07/usb-audio.log) is recorded here.

## Open checks and access limits

- Full LCD readback timed out over the temporary COM bridge. A raw diagnostic
  received rows 0–89 and a partial row 90, then normal firmware messages. No
  complete screenshot was produced, so this is not screen-layout verification.
- USB/IP could not reclaim application-mode USB from Windows (`Device busy
  (exported)`). Force binding requires Windows administrator privileges and
  was denied by the OS. No host application was stopped. The board remains
  owned by Windows and ready for Desktop use.
- Wi-Fi-only wake/command behavior and repeated quiesce/resume still need a
  run without Windows Micro traffic. Adapter power/cold boot requires a
  physical ownership/power change.
- Physical taps, six Desktop assignments/remapping/focus, native Mic/Send,
  Approve/Reject and actual Desktop dictation remain user/host interaction checks.
- BLE pairing, notification/report compatibility and orchestrator voice have
  automated software evidence but no physical BLE validation in this run.

This record proves flash, live USB RPC/status handling and raw host PCM. It
does not prove all connectivity acceptance requirements.
