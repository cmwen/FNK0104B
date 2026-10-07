# Desktop voice and the board microphone

## Connectivity behavior (0.6.0)

| Connection verified by protocol reply | Screen / controls | Speech |
| --- | --- | --- |
| No Micro session, including USB power or enumeration only | Existing Wi-Fi bridge agents and quotas | Hi ESP and bridge recording |
| USB Micro | Six Desktop slots, native agent and command keys | UAC1 microphone; local wake/commands/AFE paused |
| BLE Micro, with no USB Micro | Six Desktop slots, native agent and command keys | Hi ESP; Wi-Fi STT through orchestrator intake only |

USB takes precedence when both Micro sessions are discovered. Discovery requires
a valid `device.status`, `sys.version` or `v.oai.thstatus` request and the complete
matching reply being sent. This is protocol compatibility, not authenticated app
identity. Quiet established sessions remain Micro until their physical
connection ends; reliable Desktop app-exit detection is UNKNOWN.

Agent tiles emit AG00–AG05 and follow Desktop key configuration. The device
displays Desktop-provided slot color/brightness without inventing status meanings
or thread titles. Tap the Micro header to switch between agents and six command
keys: Fast, Approve, Reject, Fork, Mic, Send. Approve/Reject apply to Desktop's
current request; there is no bridge approval path. All keys use press/release
events, allowing host mappings and double taps. Mic and Send remain in the footer.

USB speech is raw **16 kHz, mono, PCM16 UAC1**. Select the board microphone in
the host's input settings, grant Desktop microphone access, and configure its
Mic key for push-to-talk or supported voice behavior. WakeNet, MultiNet and AFE
are quiesced before USB samples are enabled; raw ES8311/I2S capture continues.
The waveform is board input level, not a Desktop transcript indicator.

BLE advertises **Codex Micro** and retains the monitor settings service. Hold
the Micro/Codex header for three seconds to open a 60-second pairing window.
Bonded peers can reconnect. BLE speech always posts
`/v1/voice?route=orchestrator`; selecting a Micro slot cannot change its target.
A missing orchestrator produces an explicit error. BLE exposes vendor report 6
Input/Output/Feature with encrypted access and requires MTU >= 66 for a 63-byte
notification. Retail descriptor parity and Desktop BLE compatibility are
**unverified**; this first implementation exposes the vendor control collection.

Automatic first-boot Wi-Fi provisioning yields to USB Micro discovery with one
restart, skipping setup on the next boot. Explicitly requested Wi-Fi setup
remains open. Provisioning owns BLE until reboot. With no battery, unplugging
USB powers the device off; it is a cold-boot test rather than live fallback.

## Verification boundary

Earlier owner feedback confirmed USB Micro recognition, and earlier records
cover USB enumeration and the adaptive UI. They do not verify this revision's
six keys, host PCM, repeated speech transitions or BLE HID. See
[connectivity validation](connectivity-modes-validation.md) for current build
evidence and the remaining physical checks. This revision was subsequently flashed and a five-second Windows UAC recording
confirmed 80,000 nonzero-containing PCM samples. Physical keys, Desktop dictation,
Wi-Fi speech transitions and BLE remain open; see the
[device record](../test/hardware/connectivity-modes-2026-10-07.md).

## Diagnose “Mic unavailable”

The original label meant `monitor_speech::ready()` was false, which includes
microphone, task allocation, model, grammar and AFE failures. It did not indicate
that USB audio was missing. The exact original failure remains unverified. Recovery diagnostics now confirm
an internal-RAM allocation failure during WakeNet creation in the combined audio
image; the isolated raw microphone diagnostic remains stable.

With the new firmware, local microphone and recognition state are separated:

```text
monitor_audio startup=ready usb_audio=ready
monitor_audio ready=1 reason=none level=... usb_stream=... usb_dropped=...
monitor_speech state=ready ...
usb_mic stream=on rate=16000 channels=1 bits=16
codex_hid event=microphone action=press
codex_hid event=microphone action=release
```

`monitor_audio ready=0` identifies a physical capture/init issue. A healthy raw
microphone beside `monitor_speech state=error reason=...` identifies a recognition
pipeline failure. Repeated rising USB drop counts need investigation before
claiming sustained streaming quality. Host waveform/transcription acceptance
must be tested physically; a build pass does not prove those paths.

## Build and verify

Use PlatformIO Core CLI:

```sh
pio run -e codex-audio-diag
pio run -e codex-monitor
pio test -e native
pio device list
pio run -e codex-monitor -t upload --upload-port <port>
pio device monitor -p <port> -b 115200 --dtr 1 --rts 1
```

Existing generated `sdkconfig.codex-monitor` must have
`CONFIG_TINYUSB_AUDIO_ENABLED=y`; back it up and regenerate from defaults if it
predates this change. The standalone diagnostic is intentionally independent of
Wi-Fi and speech; build it before testing the combined firmware.

Runtime acceptance:

- [x] USB microphone enumerates alongside HID and CDC
- [x] Raw microphone starts and detects speech
- [x] Local speech recognition starts
- [ ] Host recording contains board microphone PCM
- [ ] Micro voice sends both ACT10 edges
- [ ] Configured Desktop Voice Chat starts on a tap
- [ ] Board level meter follows speech
- [x] Wi-Fi/SSE remains connected with HID and local speech
- [ ] Wi-Fi voice submission works during USB recording
- [ ] USB reconnect restores input/audio without stale state

The update has been uploaded with hash verification. USB descriptors have been
inspected, but microphone PCM and Desktop Voice Chat are not yet verified.
Recovery monitoring confirmed a panic reset during speech model allocation.
The latest recovery image reaches speech ready, raw microphone ready, Wi-Fi
connected and bridge stream connected. Desktop microphone/voice acceptance is
still pending. Results are in the
[validation record](../test/hardware/codex-audio-2026-10-04.md).

Windows recovery initially found Code 10 on separate audio interfaces. The final
flashed image adds their IAD grouping and Windows now reports one healthy
`TinyUSB UAC1` audio device and `Microphone (TinyUSB UAC1)` input endpoint. HID
discovery/reply, local microphone/speech and Wi-Fi workers passed the post-flash
health check. Select that Windows microphone input for the pending Desktop voice
and recording tests.

## Official references and findings

The [validation record](../test/hardware/codex-audio-2026-10-04.md) documents the
WakeNet RAM failure, Windows Code 10/IAD fix, upload recovery, final firmware hash
and remaining physical tests. The [reference index](../knowledge/references.md#usb-microphone-recovery-references-2026-10-04)
links the official Espressif, Microsoft and Codex Micro documents. The HID wire
protocol remains unofficial; official user guidance does not establish support
for this board's compatibility identity.

Build the IDF environments sequentially: they share downloaded managed components.
