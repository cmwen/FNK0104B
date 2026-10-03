# Offline voice recorder

An independent FNK0104B touchscreen recorder using WakeNet10 (“Hi ESP”),
VADNet1 medium and Opus encoding. All audio stays on the SD card.
PlatformIO Core CLI remains the build/upload interface.

![Recorder UI host preview](../../docs/images/recorder-ui.png)

## Controls

- Insert a FAT16/FAT32 microSD card before boot; the app never formats it.
- Say **Hi ESP**, then speak. Recording begins at the wake detection and
  saves automatically after about **three seconds of continuous silence**:
  VADNet debounces about one second, then the recorder waits another two.
  Resuming speech resets the silence timer. Speak promptly after waking.
- **REC** also starts recording; **STOP** saves it immediately. Both start
  paths use the same silence stop and ten-minute maximum.
- Select a filename, then **PLAY**. **STOP** under playback stops output.
  Attach a speaker to the board’s PH1.25 connector; the firmware cannot
  detect whether a speaker is physically attached.
- **PREV/NEXT** browse pages of three files. The newest 128 recordings are
  shown; older recordings remain on the card.

Touch is polled every 10 ms in a separate task, ahead of compression.
Register reads use repeated START without per-register software sleeps.
Rendering wakes on state changes and transfers only changed screen rows;
touch events bypass the periodic microphone-meter update interval.

The screen shows recorder state, elapsed recording time, a microphone meter,
VAD speech/silence status and saved files. Playback suppresses wake detection
and recording; after playback, the recorder rearms following a 1.5-second
cooldown. AEC is disabled because this firmware uses separate recording and
playback modes. It has no transcription or command vocabulary.

Capture is 16 kHz mono signed 16-bit PCM. Opus uses 20 ms frames, 24 kbps CBR,
VOIP mode and complexity 0. Recordings are standard Ogg Opus files such as
`/recordings/REC00000001.opus`, playable on a computer. Ogg overhead makes the
expected size approximately 16 MB/hour, versus 115 MB/hour for raw PCM.
A 512 ms pre-roll keeps audio immediately before the trigger, including part
of the wake phrase. Continuous AFE audio is saved without appending its
repeated VAD cache. Silence is retained within each recording, not removed.
The six-second no-speech timeout applies when VAD has not reported speech
since recording started; a wake phrase’s tail can count as speech.

## Build and upload

```sh
pio run -e recorder
pio device list
pio run -e recorder -t upload --upload-port <port>
pio device monitor --port <port> -b 115200
```

First exercise the new IDF I/O combination with the
[recorder I/O diagnostic](../21-recorder-io-diag/README.md).
The existing `sd`, `audio-diag`, and `speaker-diag` apps remain independent
capability diagnostics. Do not infer recording reliability from a build.

## Storage and flash behavior

The app mounts the verified four-bit SDIO bus. A separate storage task on
core 1 compresses and writes queued audio; feed/AFE/recognition run on core 0.
The 128-block PSRAM queue buffers about four seconds of continuous audio.
If capture fails or the queue overruns, the UI reports an error and the
incomplete file stays marked `.part`. SD write failures also retain the
partial file. Restart after resolving the error. Partial files are not listed
as completed recordings and are not automatically recovered.

Files use exclusive creation and increasing sequence numbers, preserving
both existing recordings and interrupted filenames across restarts. Only
after encoding, Ogg EOS, flush, sync, close and rename succeed does a file
appear in the recording list. Removing power or the SD card during a write
can still damage the card filesystem; this is not a transactional filesystem.
No automatic deletion, formatting or network access is performed.

**Uploading from an Arduino app changes the partition table.** Recorder uses
exactly the speech diagnostic layout: 24 KiB NVS at `0x9000`, PHY data at
`0xf000`, a 6 MiB factory app at `0x10000`, and model data at `0x610000` through
the end of 16 MiB flash. Uploading models overwrites the former Arduino OTA
slots/FATFS region; there is no OTA slot or internal recording filesystem.
Back up required old flash data first. Switching from `speech-diag` preserves
its partition boundaries but replaces firmware and models with recorder
models. Upload includes bootloader, partition table, application and models;
uploading only `firmware.bin` is insufficient. No full-chip erase or security
change is needed. SD recordings are separate from the flash layout.

## Validation and open device checks

Host verification on 2026-10-03:

- `pio run -e recorder`: builds with pinned PlatformIO Espressif32 7.0.1,
  ESP-SR 2.5.5 and esp_audio_codec 2.5.0.
- `pio run -e recorder-io-diag -e speech-diag -e speaker-diag`: builds pass.
- Recorder model image is 756,981 bytes, containing WakeNet10 and VADNet;
  MultiNet is omitted to reserve CPU and memory for recording.
- `pio test -e native`: 17 tests pass, including Ogg CRC, packet boundaries,
  malformed/truncated pages, silence reset, no-speech grace and duration limit.
- `python3 scripts/recorder_ui_preview.py`: all six UI states fit 320×240
  using the actual drawing code and board font; the image above is a host preview.
- `python3 scripts/check_recorder_opus.py`: nine files produced by the actual
  Ogg writer with host libopus decode in FFmpeg with exact sample counts,
  including very short and partial final frames. Requires C++, pkg-config,
  libopus and FFmpeg. This verifies container timing, not the ESP encoder.

Recorder was built and flashed on 2026-10-03 through PlatformIO at
`/dev/ttyACM0`, with verified bootloader, partition, model and app hashes.
The first encoding attempt exposed an undersized 16 KiB codec task stack;
Espressif documents about 40 KiB for encoding. The corrected firmware uses
48 KiB. After reflash, 115200-baud serial showed three completed SD saves
(`REC00000001.opus` through `REC00000003.opus`, with 33,280, 33,792 and
65,024 captured samples) without another observed panic. Codec stack
high-water reports retained 28,652 bytes. Idle consume maxima were about
9.2–11.8 ms per 32 ms audio frame. The existing speech partition boundaries
were retained; no full-chip erase or security changes were performed.

Serial also reported `playback=done file=REC00000003.opus`, followed by
a return to the wake state and recovery of the temporary playback memory.
This verifies SD mounting, capture, encoding, completed saves and the
decoder/I2S playback path in a short run. Exported device files have not been checked in a desktop player.
Audible speaker output, physical touch appearance/responsiveness and sustained
recording throughput remain **UNKNOWN**. Check
serial for `ready`, `recording_started`, `saved`, `playback` and storage errors.
Test quiet speech, wake followed by silence, manual stop, repeated clips,
playback cancellation, reboot with existing files, full/missing SD card,
damaged files and a long recording. Verify exported files in a desktop player.

UI responsiveness update on 2026-10-03: `recorder`, `recorder-io-diag` and
`speech-diag` builds passed, and the updated recorder was flashed with verified
hashes. Serial measured touch-read maxima of 488–1,294 microseconds and
accepted file selection, Play and Stop with sampled-event ages of
10,563–32,565 microseconds. Playback completed and returned to the wake state.
A subsequent REC/STOP touch sequence saved `REC00000004.opus` with
75,264 samples and 28,620 bytes of codec stack remaining.
These are short-run input timing measurements, not full display-latency bounds.

Silence timeout update on 2026-10-03: automatic saving now requires two
seconds of continuous VAD silence after VADNet's approximately one-second
debounce, giving roughly three seconds total. Resumed speech resets the hold.
The recorder build and all 17 native tests passed; upload hashes were verified
and serial monitoring showed normal wake-state audio processing after reboot.
The new pause duration still needs a spoken device test.

The onboard player intentionally accepts this recorder’s bounded single-stream,
one-packet-per-page format and settings; it is not a general imported Opus
music player. CRC and sequence checks reject damaged pages.

Sources: [Espressif audio codec](https://components.espressif.com/components/espressif/esp_audio_codec/versions/2.5.0/readme),
[AFE/VAD](https://docs.espressif.com/projects/esp-sr/en/latest/esp32s3/audio_front_end/README.html),
[Ogg Opus mapping](https://www.rfc-editor.org/rfc/rfc7845), and the repository’s
verified [hardware](../../docs/hardware.md) and [pins](../../docs/pins.md).
