# SD photo frame

See the [development journey](../../docs/photo-frame-journey.md) for the initial
BMP experiment, later codec support and optional monitor integration, in order.

Full-screen landscape slideshow for the FNK0104B's verified 320×240 display.
Reads `/photos` on the SD card, sorts filenames, and rotates every ten seconds.
Tap anywhere to advance. Serial commands at 115200 baud: `n` next, `p` pause/resume,
`r` rescan. No caption overlays obscure the photos. Up to 128 files are indexed.
Unsupported or truncated files are skipped; missing cards/files show setup advice.
Single-photo slideshows remain still. Card access is read-only and never formats.

## Photos

Copy the `photos` directory from `sd-card/` to the root of a FAT16/FAT32 card.
Insert the card before powering on or resetting. To change cards, power off first.
Do not remove a card during reads. For files added to the same mounted card,
`r` rescans; reset after a card swap.

Firmware 0.2.0 accepts these formats (extensions are case-insensitive):

| Format | Support | Limits |
|---|---|---|
| JPEG (`.jpg`, `.jpeg`) | Baseline and progressive, color/grayscale | Up to 1024×1024, 2 MiB file |
| PNG | RGB, RGBA, grayscale and palette; transparency over black | Up to 1024×1024, 2 MiB file |
| WebP | Lossy/lossless still images; transparency over black | Up to 1024×1024, 2 MiB file; animation rejected |
| BMP | Uncompressed RGB24, top-down or bottom-up | Up to 320×240 |

JPEG/PNG/WebP larger than the display are scaled to fit while preserving aspect
ratio, with black letterboxing. Small images stay at their original size and are
centered. Scaling uses nearest-neighbor sampling; exact 320×240 files give the
best results. JPEG EXIF rotation is not applied; PNG animation is not played.
Unsupported/corrupt files or images that exceed the memory budget are skipped.

Compressed-image buffers use PSRAM, with a 6 MiB decoder allocation ceiling plus
the encoded input (up to 2 MiB). Allocation failure is handled without falling
back to internal RAM. The Arduino loop task has a 32 KiB stack for nested decoder
tables. A full-screen RGBA image is 307,200 bytes. BMP still streams
rows with about 1.6 KiB of pixel buffers. Hardware drawing and PSRAM access live
in `lib/fnk0104b`; the hardware-independent codecs live in `lib/image_codec`.
The shared panel setup preserves the device-verified INVON colors.

Copy supported photos directly into `/photos`, or prepare exact screen-sized
files (requires Python Pillow):

```sh
python3 scripts/prepare_frame_photos.py vacation.jpg garden.png --format jpg --output-dir /path/to/card/photos
```

The helper applies EXIF orientation, center-crops to 4:3, resizes to 320×240,
and writes the selected format without overwriting existing files. `--format`
accepts `bmp` (default), `jpg`, `png` or `webp`. Each full-screen BMP is 230,454
bytes; compressed files are usually smaller. The samples in `sd-card/photos/`
now include all four formats; `sd-card-formats.zip` packages that folder.
Sample generation prompts are recorded in `assets/prompts.md`.

## Build and upload

```sh
pio run -e photo-frame
pio device list
pio run -e photo-frame -t upload --upload-port /dev/ttyACM0
pio device monitor --port /dev/ttyACM0 -b 115200
```

Use the port actually discovered. Upload replaces the running app with this
standalone photo frame and writes the existing Arduino `app3M_fat9M_16MB.csv`
layout (two 3 MiB OTA app slots and 9.9 MB onboard FATFS). When replacing the IDF
monitor/recorder, its app/model partition boundaries are replaced; reflash that
environment to restore it. SD photos are separate from onboard flash. No full
flash erase or eFuse/security change is needed.

## Validation

Host tests cover padded rows, both BMP orientations, truncated/unsupported files,
dimension limits and primary RGB565 colors. Serial reports `photo_count`,
`photo_shown`, filename, index and render time. On 2026-10-11 the PlatformIO build,
all 32 native tests and 13 firmware-CI host tests passed. Upload verified all image
hashes; serial at 115200 confirmed firmware 0.1.0, touch-controller detection and
four-bit SD mounting. The owner subsequently confirmed the original photo frame
works. Version 0.2.0 adds native tests using actual baseline/progressive JPEG,
RGB/paletted/transparent PNG and lossy/lossless/transparent WebP fixtures,
repeated decode/free cycles, malformed/oversized/animated inputs and forced
allocation failures. See the dated hardware record for device verification.
All 35 native tests pass. The final 0.2.0 upload verified hashes and serial
confirmed continued BMP playback; compressed-format device testing awaits the
new files on the card. See [codec evidence](../../test/hardware/photo-frame-codecs-2026-10-11.md).
