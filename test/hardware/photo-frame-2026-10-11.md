# Photo frame startup — 2026-10-11

- PlatformIO Core CLI `pio run -e photo-frame`: passed on the pinned platform.
  Firmware 402,032 bytes on disk; static RAM 23,656 bytes.
- `pio test -e native`: 32/32 passed, including padded BMP rows, orientations,
  truncation, unsupported encoding/dimensions and RGB565 primary colors.
- Firmware-CI host tests: 13/13 passed. Catalog recognizes `photo-frame`.
- Two generated sample photos independently checked as 320×240, RGB24,
  uncompressed BMP, 230,454 bytes each.
- Host device discovery initially found 303a:8360 (running monitor) on ttyACM0.
  The first upload's 1200-baud reset changed the device to ROM 303a:1001; port
  discovery failed transiently before any flash write. Retrying after discovery
  succeeded with all image hashes verified and watchdog reset.
- Upload used the existing Arduino app3M_fat9M_16MB layout, replacing the IDF
  monitor layout. No erase-all operation or security/eFuse change.
- PlatformIO serial monitoring at 115200, followed by `r`, received startup and
  card status. The card mounts but `/photos` is absent; no photos were written
  or modified on the card. Firmware now displays instructions to add photos.

Captured serial:

```text
--- FNK0104B firmware ---
firmware=photo-frame
version=0.1.0
chip_model=ESP32-S3
flash_bytes=16777216
psram_bytes=8386295
free_heap_bytes=360836
status=running
touch_i2c=ready address=0x38
sd_pins=configured mode=4-bit
sd_status=mounted
[E][vfs_api.cpp:105] open(): /sdcard/photos does not exist, no permits for creation
```

Physical photo rendering, colors, tap delivery and sustained slideshow remain
UNKNOWN. Copy the sample ZIP's `photos/` folder onto the SD root, then reset.
