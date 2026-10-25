# Whistle offline transcription 0.1.0

Standalone speech-to-text firmware for the FNK0104B's onboard microphone and
320 × 240 landscape touchscreen. Tap **Record**, speak, then tap **Stop and
transcribe**. Recording stops automatically at five seconds. The display shows
microphone level, clip duration, processing time, detected language and the last
transcript. PAGE 1 / PAGE 2 switches between two transcript pages. Starting a new
recording replaces the previous result. Nothing is saved to flash or SD.

Inference runs locally without Wi-Fi, a phone or a server. Supported model
languages are English, German, French, Spanish, Italian, Dutch and Polish;
Chinese is not supported by this model. Language detection is automatic. This
build uses unbiased transcription, without the upstream LED demo's color-word
bonus or command-based language retries. It does not actuate the rear LED.

The shared diagnostic font displays uppercase Latin letters and limited ASCII
punctuation. Non-ASCII codepoints appear as `-`; original UTF-8 text is printed
as `TXT ...` over serial. Results are capped at 96 tokens / 511 UTF-8 bytes and
may be truncated. This is short-clip transcription, not streaming dictation.
One physical 4.92-second English clip took 14.832 seconds to transcribe. Allow
several seconds after Stop; broader accuracy and timing remain unmeasured.

## Prepare, build and install

Use PlatformIO Core CLI. No Arduino IDE or standalone `idf.py` installation is
required. The isolated environment uses the existing pinned
`platformio/espressif32@7.0.1`, IDF 5.5.5 and Xtensa GCC 14.2 packages. Its own
lockfile isolates dependency resolution from existing apps. The shared
PlatformIO/IDF linker helper is reused.

Prepare the model once with Python 3.11+ and NumPy (a virtual environment is
recommended). Ordinary firmware builds need neither NumPy nor the model file:

```sh
python3 -m venv .pio/whistle-venv
.pio/whistle-venv/bin/python -m pip install numpy==2.2.6
.pio/whistle-venv/bin/python scripts/prepare_whistle_model.py
pio run -e whistle
pio device list
pio run -e whistle -t upload --upload-port <port>
pio device monitor --port <port> -b 115200
```

For an existing official model, add `--source /absolute/path/whistle.cact` to
the preparation command. `--verify` checks an existing prepared image. The
source revision and source/image SHA-256 values are pinned in the preparation
script. Downloads and generated images stay under ignored `.pio/models/whistle/`.
The conversion tools are vendored from the credited upstream commit. Full CLI
uploads include the model at `0x210000` and reject missing or modified images.
The browser installer does not offer this environment; CI builds the firmware
without downloading weights. Read the partition effects below before upload.

## Memory and scheduling

The depth-4 image is 14,515,887 bytes, read directly from mapped flash. Its
2,164,536-byte output head is cached in PSRAM. Recording uses 320,000 bytes of
PSRAM; the existing display uses a 153,600-byte framebuffer and an internal
DMA stripe. Inference gets a 48 KiB task stack and a 16 KiB second-core worker.
A five-second synthetic host probe measured 5,153,032 bytes of peak tracked
engine/clip/encoder allocations, including the head cache. Host pointer sizes,
platform allocations and untracked tokenizer/small scratch differ from the
board, so this is a budget estimate, not device heap evidence.

Before inference, the firmware requires at least 4 MiB free PSRAM and a 3 MiB
largest free block. This conservative check protects the unchanged upstream
engine, which assumes some internal allocations succeed. Board memory behavior
still needs verification. Both row workers yield periodically so idle tasks
can run; task watchdog protection remains enabled. A higher-priority display/
touch task shows elapsed processing time. Microphone reads pause for inference;
200 ms of queued audio is discarded before returning to idle. The quietest idle
window estimates the noise floor; a 12 dB speech gate avoids most silent clips.
The first 50 ms and last 30 ms of a recorded clip are omitted for touch transients.

## Partition effects and recovery

**A full upload replaces the partition table and overwrites existing app/model
regions.** There is one 2 MiB factory application at `0x10000`, followed by the
`0xdf0000`-byte model region at `0x210000` to the end of the 16 MiB flash. There is
no OTA slot or on-flash filesystem. NVS retains the monitor boundary at `0x9000`,
size `0x5000`; Whistle does not initialize or write NVS. PHY data is at `0xf000`.
The old OTA metadata region is no longer described by this table. Existing
flash filesystem contents cannot be preserved by this layout; back up anything
needed before switching. SD contents are untouched.

To return to the monitor, build and upload the complete `codex-monitor`
environment, including its speech model images. A previous firmware-only OTA
image cannot restore this layout. No erase-all, eFuse or security changes are
needed. Monitor preferences should remain in the unchanged NVS region, but
cross-firmware preservation has not been tested in this session.

## Verification and credits

See [dated evidence and open device checks](../../test/hardware/whistle-2026-10-25.md).
The complete Whistle environment was installed on the attached board on
25 October 2026 with all four image hashes verified. A 40-second PlatformIO
serial capture confirmed model loading, ready state and stable idle heap
(5,363,580 PSRAM bytes free). One physical English test produced the exact sentence “This is a test of
offline speech recognition.” in 14.832 seconds for 4.92 seconds of audio; the
owner confirmed the transcript appeared. Free PSRAM returned to 5,363,580 bytes
after inference, with a 2,819,660-byte observed minimum. Broader recognition
accuracy, manual Stop/page coverage, detailed screen appearance, multilingual
behavior and sustained stability remain open checks.

The portable C99 inference engine, ESP32-S3 int8 optimizations and depth-reduction
tools come from **[andrisgauracs/whistle-esp32](https://github.com/andrisgauracs/whistle-esp32)**,
pinned to `383e0088d7140a89a9111aa985671190a9048aa3`. **Cactus Compute** created
[Whistle and its weights](https://huggingface.co/Cactus-Compute/whistle).
Both upstream code and model are Apache-2.0. Full source attribution and the
retained license are in [lib/whistle](../../lib/whistle/README.md). This repository
adds FNK0104B board integration, touch UI and the PlatformIO workflow; it does not
claim authorship of the model or inference engine.
