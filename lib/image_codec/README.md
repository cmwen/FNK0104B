# Still image decoder

Hardware-independent JPEG/PNG/WebP decoding into RGBA, limited to 1024 pixels
per axis, 2 MiB encoded input and 6 MiB decoder allocations. Allocator hooks let
the FNK0104B board support use verified PSRAM; host tests use ordinary memory.
Single-threaded; configure allocator hooks before decoding and keep them stable
while decoded images are alive. Allocation failures return an error.

Pinned upstream sources:

- stb_image v2.30 from [stb commit 2c980bb59875b0d32144a71867fbdebb2f77cd20](https://github.com/nothings/stb/tree/2c980bb59875b0d32144a71867fbdebb2f77cd20).
  The unmodified header includes its MIT/public-domain license. Only JPEG and
  PNG are compiled; stdio and SIMD are disabled.
- [libwebp 1.6.0](https://github.com/webmproject/libwebp/tree/v1.6.0).
  Retains decoder C sources and common DSP/utils sources listed by upstream's
  Makefile.am; headers and COPYING/PATENTS/AUTHORS accompany them. Encoders,
  SIMD implementations, mux/demux and tools are omitted. Local config.h selects
  portable single-threaded C. The only upstream source changes are utils.c's
  three allocation calls and allocator-header include, routing WebPSafeMalloc,
  WebPSafeCalloc and WebPSafeFree through the same bounded allocator as stb.

Both lossy and lossless still WebP are decoded. Animated WebP is rejected;
APNG animation is not played. JPEG EXIF orientation is not applied by firmware.
Resize/orient photos with the preparation helper when needed.
