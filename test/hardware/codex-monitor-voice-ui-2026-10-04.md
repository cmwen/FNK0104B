# Codex monitor voice and status update — 2026-10-04

The user authorized flashing, adding screenshots/documentation, committing and
pushing this update. PlatformIO Core discovered the FNK0104B's Espressif
303a:1001 USB serial device at `/dev/ttyACM0`.

## Build and upload

`pio run -e codex-monitor` passed, with 2,998,605 flash bytes and 122,188 static
RAM bytes. All 20 native and 48 bridge regression tests passed. The native
checks cover reset countdown endpoints, silence grace/reset behavior, initial
speech wait, capture limit and microphone-level scaling.

`pio run -e codex-monitor -t upload --upload-port /dev/ttyACM0` succeeded and
verified the bootloader, partition table, speech model and application hashes.
The existing speech partition boundaries were retained: NVS at `0x9000` with
size `0x5000`, factory app at `0x10000` with size `0x600000`, and models at
`0x610000` with size `0x9f0000`. No full-chip erase or security-setting change
was performed. Saved Wi-Fi credentials survived.

## Serial inspection

PlatformIO Core's serial monitor at 115200 baud reported:

```text
firmware=codex-monitor
version=0.5.0
flash_bytes=16777216
psram_bytes=8388608
touch_i2c=ready address=0x38
monitor_ble advertising_data status=0
monitor_ble scan_response status=0
monitor_ble advertising_started status=0
Quantized MultiNet7 search method: 2, time out:12.0 s
monitor_speech state=ready wake=Hi_ESP wakenet=wn10_hiesp vadnet=vadnet1_medium multinet=mn7_en
monitor_wifi connected=true status=3
monitor_stream state=connected
monitor_status integration=connected agents=1 first_state=running five_hour_used=17 weekly_used=46
monitor_speech state=wake afe_frames=474 max_inference_us=6424 frame_us=32000 heap=7511 psram=4463692
monitor_speech state=wake afe_frames=788 max_inference_us=6040 frame_us=32000 heap=7511 psram=4463692
```

No panic, audio-fetch failure or speech-model initialization failure was observed
in this short inspection. Internal heap remains tight; this is not a sustained
stress test.

## Screenshot and interaction limits

The repository's `capture_monitor_screen.py` uses PlatformIO's 115200-baud
serial terminal and the firmware's LCD readback command to collect and validate
all 240 RGB rows. The current actual-board image is saved at
[`docs/images/codex-monitor-screen.png`](../../docs/images/codex-monitor-screen.png).
It is separate from the sample-data host previews in the app README.

Startup, the configured twelve-second recognition timeout and concurrent live
status/speech processing are device-verified. Spoken command accuracy, physical
meter responsiveness, longer thinking pauses, thirty-second recording and
selection/submission to a running agent remain **UNKNOWN** until exercised on
the board. A screenshot verifies the displayed pixels, not those interactions.
