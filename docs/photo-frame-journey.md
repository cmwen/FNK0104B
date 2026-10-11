# Photo frame and monitor idle-photo journey

All development stages below occurred on **11 October 2026, Australia/Sydney**.
Their order comes from the conversation and dated hardware records. The stages
were committed together as [ea00021](https://github.com/cmwen/FNK0104B/commit/ea00021)
at 23:00:14 AEDT; that is the commit time, not the time of each hardware check.

| Order | Change or observation | Evidence and limits |
| --- | --- | --- |
| 1 | Build standalone `photo-frame` 0.1.0 and generate two 320 × 240 sample photos. | PlatformIO build/upload and 32 native tests passed. SD mounted, but `/photos` was absent, so the first run showed setup instructions. Rendering was UNKNOWN at this stage. |
| 2 | Put the BMP samples on SD and confirm the slideshow works. | The owner reported “Nice, it works.” This supersedes the initial missing-folder observation; it does not establish every touch or timing behavior. |
| 3 | Extend standalone firmware to 0.2.0 with JPEG, PNG and still WebP. | Build/upload and 35 native tests passed, including real compressed fixtures and memory-failure handling. Serial confirmed continued playback of the two BMPs. Compressed-image playback on the physical SD card remained UNKNOWN. |
| 4 | Design optional idle photos for the Codex monitor and prepare images in the browser before upload. | Monitor 0.8.0 adds normal UI, blank below the bar, or photos; idle delay, photo interval and the existing screen-off timer work together. The live 44-pixel usage bar leaves a 320 × 196 photo area. The default remains normal UI. |
| 5 | Diagnose the monitor SD write failure. | Mounting succeeded, but creating `/monitor-photos` failed with `EINVAL`: the monitor had FAT long filenames disabled. Bounded heap-based long-filename support fixed directory creation and writes. This was a filesystem configuration problem, not proof of a failed SD card or exhausted memory. |
| 6 | Test prepared-file transfers, rendering and resource use on the board. | An incomplete transfer was rejected. Two 125,456-byte frames committed through the same processor used by BLE, exercised through serial. Actual LCD readback showed photos below the live usage bar. USB audio remained available with zero reported drops. These are serial/USB results, not physical BLE-upload results. |
| 7 | Fix wake handling, flash the final monitor build and check restoration. | Photos survived the final flash. Normal controls returned after preview, and PSRAM was released. Saved idle mode stayed 0. The board was left on the normal monitor UI. |

## Why the monitor prepares photos differently

The standalone app can devote PSRAM and a larger task stack to image decoders.
The monitor also runs networking, USB, audio and speech workers, with much less
internal memory available. Its browser crops, resizes and converts JPEG/PNG/WebP
to a fixed RGB565 frame instead of adding those decoders to the monitor. Each
file is 125,456 bytes; the current pixel cache is 125,440 bytes in PSRAM.

The final USB Micro check kept 36,919 bytes of free internal heap. Free PSRAM
went from 8,149,348 bytes with a frame to 8,276,324 bytes after leaving preview.
Recognition was paused in that mode. Active Wi-Fi speech recognition has a
different memory profile and was not validated with photos.

## Firmware and evidence boundaries

Standalone photo-frame tests switched from the monitor to the ordinary Arduino
partition layout. Returning to the monitor restored its documented partitions
and speech models. No SD formatting, erase-all, eFuse or security changes were
performed. `/photos` and `/monitor-photos` are separate directories; BMP or
compressed files in the first are not monitor prepared frames in the second.

Physical BLE settings saving/upload, real disconnect cancellation, touch wake,
automatic idle/screen-off acceptance, compressed-image playback in standalone
firmware and monitor photos with active Wi-Fi speech recognition remain UNKNOWN.
The optional monitor idle mode stays off by default. A passing build, native
decoder test, browser mock or serial upload does not close these physical checks.

## Where to find the details

- [Standalone setup, supported formats and sample photos](../apps/27-photo-frame/README.md)
- [Monitor configuration, browser preparation and transfer protocol](monitor-photos.md)
- [Initial standalone startup record](../test/hardware/photo-frame-2026-10-11.md)
- [Codec update and subsequent BMP playback](../test/hardware/photo-frame-codecs-2026-10-11.md)
- [Monitor transfers, LCD captures and final memory check](../test/hardware/monitor-photos-2026-10-11.md)
- [Current monitor readiness](monitor-readiness.md)

The public [build journal](https://cmwen.github.io/FNK0104B/journey/) also places
this work after the October 10 avatar option and October 11 Desktop-status
animation change. Journal milestones use their actual commit timestamps and
are presented newest first, independently of the dated hardware records.
