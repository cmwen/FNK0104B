# Codex monitor restored after Whistle — 25 October 2026

## Requested operation

The owner requested the completed Whistle work committed/pushed and the board
returned to Codex monitor. Whistle implementation and its physical English
transcription evidence were committed as `22076b0`; verified journey provenance
was committed as `413ffd5`. Both were pushed to `origin/main` before restoration.
The pre-existing untracked subscription-experiment document and old local logs
were excluded from these commits.

## Build and full upload

The named PlatformIO `codex-monitor` environment built successfully using the
existing pinned espressif32 7.0.1, IDF 5.5.5 and Xtensa GCC 14.2 packages. The
application remains version 0.8.7; no monitor feature or GPIO mapping changed.
Build accounting: 3,407,849 / 4,194,304 application bytes and 154,892 / 327,680
static RAM bytes. The host suite passed 47 tests; guide build and four guide
checks passed before the push.

`pio run -e codex-monitor -t upload --upload-port /dev/ttyACM0` succeeded in
80.46 seconds. The bootloader, partition table, OTA metadata, speech model and
application each passed esptool hash verification. The supported watchdog
reset completed. See [upload log](monitor-restored-after-whistle-2026-10-25/upload.log).

| Image | Bytes | SHA-256 |
| --- | ---: | --- |
| Application | 3,408,256 | `71e214a778373f068ab6ea5e912d8da4bb4aa4845af4b2e4a2269aa0974e027d` |
| Partitions | 3,072 | `ae7c126f54ae96ad8b50f1688620aa94cd8b723ff87dd81240ea5e39dafb1fd1` |
| Speech models | 3,518,070 | `e0a43266597da7db07f066a80893ca093f6a9395e9c298af3941eaf6d3c3e833` |

The full upload restored two 4 MiB OTA application slots at `0x10000` and
`0x410000`, OTA metadata at `0xe000`, and the `0x7f0000` speech-model region at
`0x810000`. NVS remained at `0x9000`, size `0x5000`, outside all erased/written
ranges. This replaced the Whistle single-app layout/model. No erase-all,
eFuse, Secure Boot, Flash Encryption or other security change was performed.

## Runtime and LCD observations

After reset, Windows enumerated `303a:8360` Codex Micro on bus `3-1`. Reattaching
to WSL exposed `/dev/ttyACM0` as TinyUSB CDC. PlatformIO monitoring at 115200 baud
needed DTR/RTS active for this CDC implementation; inactive lines produced no
monitor diagnostics. A `health` command returned:

```text
monitor_health heap=6107 psram=4420516 wifi=3 status_task=1 voice_task=1 speech=1 speech_reason=none version=0.8.7 slots=3 avatars=1
```

Continuous wake-state AFE processing, speech/silence transitions and ready audio
were observed. Sampled USB dropped counters stayed zero, but `usb_stream=0`,
so this does not validate a real host microphone stream. Internal free heap was
6,107 bytes initially and 5,875 bytes later; sampled PSRAM remained 4,420,516
bytes. This is a short run with speech enabled, not an endurance or low-memory
OTA check. No panic or watchdog failure appeared in the capture.

A complete 320 × 240 LCD readback was decoded and inspected locally. It showed
Wi-Fi Online, live allowance data, an active agent and Wi-Fi voice controls.
The screenshot remains a local diagnostic artifact because it contains live
account/activity data. [Saved serial diagnostics](monitor-restored-after-whistle-2026-10-25/serial-health.log)
omit LCD pixels and preserve the relevant health/audio/speech observations.

Wi-Fi connected through the monitor's existing `WiFi.begin()` path, and the
avatar preference loaded enabled (its absent-key default is disabled). These
observations support saved Wi-Fi/avatar preference retention. Live bridge data
was available. The bridge token was neither read back nor printed; other saved
preferences, physical touch and audible voice behavior were not systematically
checked.

## USB ownership

The serial monitor was closed and the board detached from WSL. Removing the
forced `303a:8360` USB binding requires Windows administrator privileges; an
attempt from this session was denied by Windows. The owner was asked to run
`usbipd unbind --busid 3-1` in Administrator PowerShell. Until that is confirmed,
Desktop USB discovery after the return remains UNKNOWN; the restored board
continues running independently with Wi-Fi available.
