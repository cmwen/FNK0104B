# Codex monitor restored after HID testing — 2026-10-04

The owner confirmed direct Windows 11 touchscreen keyboard input, requested
that the HID firmware/documentation be committed and pushed, then requested
restoration of the Codex monitor. HID work was pushed as `03c56f5` before
this upload. Existing local monitor/dispatcher edits were preserved; the
monitor build includes the already-present request-ID/queued-response changes
in `apps/codex-monitor/src/main.cpp`, without adding a new monitor feature.

## Build and upload

- `pio run -e codex-monitor` passed with the pinned platform 7.0.1, IDF 5.5.5
  and GCC 14.2.0 toolchain. Flash usage: 2,998,869 bytes of the 6 MiB app;
  static RAM: 122,188 bytes.
- PlatformIO discovered keyboard TinyUSB CDC at `/dev/ttyACM0`. A bounded
  PlatformIO monitor open at 1200 baud invoked the verified ROM reset path.
  USB/IP reattachment restored the board's ROM Serial/JTAG interface on the
  discovered `/dev/ttyACM0` port. No physical button step was required.
- `pio run -e codex-monitor -t upload --upload-port /dev/ttyACM0` exited
  SUCCESS. Esptool verified bootloader, partition-table, speech-model and app
  hashes. It wrote 22,288 bootloader bytes at 0x00000000, 3,072 partition bytes
  at 0x00008000, 3,518,072 model bytes at 0x00610000, and 2,999,280 app bytes
  at 0x00010000.
- The existing monitor partition table restores 20 KiB NVS at 0x9000, PHY at
  0xf000, a 6 MiB factory app at 0x10000, and models at 0x610000. This replaces
  the keyboard's Arduino OTA/FATFS layout; its NVS boundary is preserved.
  No full-chip erase, SD access, eFuse or security-setting changes occurred.

## Runtime observations

A bounded PlatformIO serial monitor at **115200 baud** received repeated:

```text
monitor_speech state=wake afe_frames=1102 max_inference_us=6364 frame_us=32000 heap=7503 psram=4463792
monitor_speech state=wake afe_frames=1259 max_inference_us=6382 frame_us=32000 heap=7503 psram=4463792
monitor_speech state=wake afe_frames=1416 max_inference_us=6573 frame_us=32000 heap=7503 psram=4463792
monitor_speech state=wake afe_frames=1573 max_inference_us=6155 frame_us=32000 heap=7503 psram=4463792
monitor_speech state=wake afe_frames=1730 max_inference_us=6310 frame_us=32000 heap=7503 psram=4463792
```

The existing PlatformIO-based `scripts/capture_monitor_screen.py` completed
a full 320 × 240 LCD readback into a temporary verification image. Visual
inspection showed **Wi-Fi Online**, **Codex Busy**, two running agent cards,
quota/reset bars and the New Codex message microphone control. This confirms
the saved network/bridge path and live screen resumed in this short run.
No voice submission, wake-command accuracy, audible speaker or sustained-run
test was performed during restoration.

## Reproducibility

SHA-256 hashes of the local build/source used in this restoration:

| Artifact | SHA-256 |
|---|---|
| monitor firmware.bin | `6828135d6106ef15469b3667201c944225dc88af94d8c18b0afb1f39d4eeb63a` |
| speech srmodels.bin | `2911d0b98a3ac57eb523f67276741e9c370e7c134310de3b3d04789fb7d68499` |
| monitor main.cpp including existing local edits | `0aef131d1593c580f65ef61573b13b21841f1b7bd99da8d4b4b2fbfeb925d761` |

Private monitor configuration remains Git-ignored; no secrets were committed.
