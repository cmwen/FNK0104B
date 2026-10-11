# JPEG / PNG / WebP photo frame — 2026-10-11

The owner confirmed the original BMP photo frame works. Firmware 0.2.0 adds
JPEG (baseline/progressive), PNG and lossy/lossless still WebP, with alpha over
black, fit-to-screen scaling, PSRAM allocations and a 32 KiB Arduino loop stack.
The current Arduino flash partition layout and verified board pin setup remain
unchanged. No SD writes, flash erase-all or eFuse/security changes were performed.

## Checks

- Final PlatformIO build/upload passed, all image hashes verified, watchdog reset.
  App flash 493,849 bytes (15.7% of the 3 MiB slot), static RAM 24,536 bytes.
- All 35 native tests passed. Actual encoded fixtures cover baseline/progressive
  JPEG, RGB/palette/transparent PNG, lossy/lossless/transparent WebP, repeated
  decode/free cycles and 640×480 decoding/fit for all three formats. Invalid,
  truncated, animated and oversized images plus forced allocation failures and
  budget exhaustion are exercised. Allocated-byte tracking returns to zero.
- Firmware-CI host tests: 13/13 passed. Photo-frame catalog metadata updated.
- Preparation helper wrote a verified 320×240 WebP file. The new format sample
  ZIP contains eight files: two scenes each as BMP/JPEG/PNG/WebP.
- After the final upload, PlatformIO monitoring at 115200 baud and a serial `r`
  confirmed two BMPs on the inserted card and continued rendering:

```text
photo_shown=/photos/02-red-poppy.bmp index=2/2 render_ms=952
photo_count=2 limit=128
photo_shown=/photos/01-alpine-lake.bmp index=1/2 render_ms=1038
```

## Remaining device evidence

The card currently contains only the original BMPs. Compressed-image SD playback,
physical scaling/alpha/colors and sustained PSRAM stability remain UNKNOWN until
JPEG/PNG/WebP files are copied to `/photos` and checked on the device. Native tests
verify software decoders but do not establish those physical observations.
