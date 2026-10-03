# Visual WakeNet10 and voice-command diagnostic

Build and upload using PlatformIO Core CLI, without Arduino IDE or a standalone
IDF installation. `speech-diag` uses the same pinned Espressif32 platform as the
other apps, with its ESP-IDF framework and ESP-SR 2.5.5. Arduino apps retain
Arduino 2.0.17. The newer speech library requires IDF >=5.

```sh
pio run -e speech-diag
pio device list
pio run -e speech-diag -t upload --upload-port <port>
pio device monitor --port <port> -b 115200
```

Build before upload. The landscape screen shows a loading message, then
**SAY "HI ESP"**. Say **“Hi ESP”** and wait for **LISTENING** and its six-second
countdown. Then say one of the four commands printed on screen:

- “turn on the light”
- “turn off the light”
- “start listening”
- “stop listening”

A green **COMMAND HEARD** message confirms recognition. **LAST** keeps the
recognized phrase and match probability visible while the test rearms. If no
command is recognized, **NO COMMAND HEARD** briefly appears before returning to
the wake prompt. The MIC bar and peak value show microphone activity; the bottom
row counts wakes, matches and timeouts. The screen keeps the instructions and
all four commands visible throughout testing. **VADNET SPEECH / SILENCE** shows
neural voice activity detection. Speak, then pause: SILENCE appears after about
one second without speech. No touch interaction is required.

Commands only display and print their ID, phrase and probability; they do not
control a peripheral. WakeNet10 detects the wake phrase; English MultiNet7 recognizes
this fixed command vocabulary. USB serial retains detailed diagnostic logs. This is not dictation or arbitrary transcription.
No network, SD storage, audio recording export, or speaker playback is needed.
Board access remains in `lib/fnk0104b`; the codec settings follow the previously
verified microphone diagnostic. Recognition accuracy still needs a spoken
wake-word/command test on the device. Use a quiet room and speak clearly near the microphone.

The display uses Espressif's pinned ILI9341 2.1.0 driver and the verified shared
board wiring, landscape rotation and `INVON` setting. A separate task on core 1
renders at about four frames per second with a bounded snapshot queue, PSRAM
framebuffer and small internal DMA stripe buffer. Speech continues on core 0.

Every five seconds, the diagnostic prints the signal peak, counters, inference
time and free internal/PSRAM memory. Silence must not trigger commands. Try
unrelated speech, commands without a wake word, a wake word followed by silence
(timeout), repeated wake/command cycles and background audio. The worst inference
time should stay below the reported audio frame duration. If it does not,
audio may be lost; do not claim reliable recognition from a successful build.
Firmware 0.3.0 routes the single microphone through Espressif's AFE with
`vadnet1_medium` explicitly selected (128 ms minimum speech, 1000 ms minimum
silence, 128 ms VAD delay). All continuous AFE frames, including silence, reach
WakeNet10/MultiNet7; VAD reports activity without gating or clipping command
prefixes. The VAD cache repeats earlier audio and is therefore not appended.
AEC, noise suppression and automatic gain control remain disabled. AEC needs
a playback reference; this test has no speaker playback. The feed task and AFE
processing run independently of the display and recognition tasks.

`compile_req.yml` supplements ESP-SR 2.5.5's kernel selection with the unbiased
W8A16 1x1 convolution used by `wn10_hiesp`. Without this supplement, hardware
inference logged `no Conv kernel for packed key 0x0202` despite a successful
build. Firmware 0.1.1 checks that kernel at startup.

## Flash layout effect

**Uploading this environment changes the partition table.** It replaces the
Arduino two-slot OTA/FATFS layout with NVS at `0x9000` (24 KiB), PHY data at
`0xf000`, one 6 MiB factory app at `0x10000`, and a model-data partition at
`0x610000` (the rest of the 16 MiB flash). No OTA or FAT filesystem is available.
The model upload overwrites that region, including data that belonged to the
previous filesystem or app slots. Do not use this layout if their contents need
to be preserved without first backing them up. No full-chip erase is required.
Existing NVS data interpretation is not guaranteed across frameworks/layouts.

`scripts/speech_models.py` packs the selected models with Espressif's own packer,
checks the partition size and includes `srmodels.bin` in normal CLI uploads.
Uploading only `firmware.bin` is insufficient. To return to an Arduino app,
rebuild and upload its named environment to restore its partition table and
firmware; previously overwritten data is not restored. This diagnostic is
built in CI but excluded from the browser flasher's Arduino-only catalog.

## USB-JTAG

The environment explicitly selects `debug_tool = esp-builtin` and USB Serial/JTAG
console, preserving native USB on GPIO19/20. It includes debug symbols and uses
`debug_load_mode = manual` so attaching does not automatically upload firmware.
After building and uploading the matching firmware:

```sh
pio debug -e speech-diag --interface=gdb
```

Use `break app_main`, `monitor reset halt`, `continue`, and `backtrace` to inspect
startup. Always match the ELF to the flashed firmware. USB/IP attachment and
Linux USB permissions are described in [development](../../docs/development.md).
The serial monitor's `dialout` access alone does not grant OpenOCD JTAG access.
USB-JTAG capability is documented by Espressif and supported by the board wiring;
actual breakpoint operation and device security/eFuse state remain UNKNOWN.
Do not change eFuses or security settings to make debugging work.

Sources: [ESP-SR](https://github.com/espressif/esp-sr),
[WakeNet10 on ESP32-S3](https://docs.espressif.com/projects/esp-sr/en/latest/esp32s3/benchmark/README.html),
[VADNet](https://docs.espressif.com/projects/esp-sr/en/latest/esp32s3/vadnet/README.html),
[Audio front end](https://docs.espressif.com/projects/esp-sr/en/latest/esp32s3/audio_front_end/README.html),
[USB-JTAG](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-guides/jtag-debugging/configure-builtin-jtag.html).

## Validation recorded on 2026-10-03

- `pio run -e speech-diag`: passed with ESP-IDF 6.0.1 and locked ESP-SR 2.5.5 dependencies.
- `pio run -e audio-diag -e hello-debug`: passed.
- `pio test -e native`: all 15 tests passed.
- Generated partition binary checked against the documented offsets and sizes.
- Packed model image: 3,230,005 bytes, containing `wn10_hiesp`, `mn7_en` and `fst`.
  Verified the packer selects the S3 `p1` model and renames its data to the
  `wn10_data` filename expected by the loader.
- PlatformIO uploader arguments inspected without upload: model image included
  once at `0x610000`, alongside bootloader, partition table and app.
- During initial build validation, `pio device list` in the sandbox showed no attached device. No upload, serial recognition test,
  physical microphone test or JTAG breakpoint test was performed.

## Device upload and runtime check (2026-10-03)

PlatformIO discovery with device access found the board at `/dev/ttyACM0`,
USB `303a:1001`, serial `B8:1F:3F:C3:9F:94`. The sandbox's device view hid
the serial node; an empty sandbox listing did not mean the board was unplugged.
Firmware 0.1.1, partition table, bootloader and models were flashed through
`pio run -e speech-diag -t upload --upload-port /dev/ttyACM0`; esptool verified
all written hashes. This applied the dedicated speech partition layout.

`pio device monitor --port /dev/ttyACM0 -b 115200` then showed repeated
`state=wake` reports without the previous Conv errors. Captures completed with
peaks of 157 and 231; inference maxima were 5,654 and 5,568 microseconds versus
a 32,000-microsecond frame. Internal free heap was 307,719 bytes and free PSRAM
was 4,881,336 bytes in both reports. This verifies capture/inference startup
and a short run, not spoken wake-word or command recognition accuracy.

## Visual test 0.2.0 (2026-10-03)

Built and flashed the visual test through PlatformIO with verified image hashes.
The Arduino `display` build and all 15 native tests also passed. A host rendering
check confirmed the instructions and labels fit 320x240. After upload, USB serial
showed normal wake/listening transitions and `command_detected=1 id=2` for
“turn off the light” while the display task ran. No display-transfer or model
kernel errors appeared in the observed output. Wake inference maxima were
10,788 and 10,488 microseconds, and a listening report reached 30,585 microseconds
for a 32,000-microsecond frame. This short run confirms the combined path runs,
not its long-term accuracy or worst-case audio throughput. Physical appearance
of the new screen has not been independently inspected.

## VADNet test 0.3.0 (2026-10-03)

Built and flashed through PlatformIO with verified bootloader, partition, model
and app hashes. The existing speech partition layout was retained. Packed model
image size is 3,518,070 bytes (esptool pads the write to 3,518,072), including
`vadnet1_medium` alongside WakeNet10, MultiNet7 and the command FST. All 15 native
tests passed. Serial at 115200 baud reported continuous AFE processing (157 feed
frames in successive five-second reports), `vad=speech` followed by
`vad=silence`, and no observed fetch, frame-size, model-kernel or display errors.
Wake inference maxima were approximately 10.3–10.5 ms per 32 ms frame. Internal
free heap stayed at 237,255 bytes and PSRAM at 4,355,732 bytes during this short
check. The same session then detected a wake and command ID 3 (“start listening”,
probability 0.210), with another speech/silence transition. Listening inference
reached 31,520 microseconds per 32,000-microsecond frame, leaving little margin.
This confirms VAD and command operation in a short run; systematic accuracy
and long-term throughput remain unmeasured.
