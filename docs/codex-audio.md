# Desktop voice and the board microphone

## What changed

The top status cell now says **Micro**, with independent HID connection state:

- **Off**: USB is not mounted.
- **USB**: USB is mounted, but no recognized discovery/status call has arrived.
- **Linked**: a `device.status`, `sys.version`, or `v.oai.thstatus` call arrived
  within the last minute.
- **Idle**: a call was previously observed during this USB connection, but no
  such call has arrived recently. This does not mean the app disconnected.

This is host protocol activity, not authenticated application identity; a
compatible probe can also produce it. The owner reported that Desktop recognized
the previous firmware as Codex Micro on 2026-10-04.

The bottom panel has two independent controls. **Micro voice** sends ACT10 press
on touch-down and release on lift, while **Wi-Fi voice** retains the existing
orchestrator recording/dispatch path. This preserves concurrent integrations.
The Micro waveform shows actual board input level while the host is capturing
or the key is held. It does not claim to show the Desktop transcript or exact
remote voice-session state.

USB now contains CDC, vendor HID and a **UAC1 microphone: 16 kHz, mono, PCM16**.
Audio is carried by USB Audio Class; the vendor HID report carries the control
key. No USB speaker, BLE audio or external pins were added. CDC needs two IN/one
OUT endpoints, HID one IN/one OUT, and audio one IN: four IN/two OUT in total.

One board-level capture worker owns ES8311/I2S reads and supplies raw PCM to USB
and the existing AFE through a bounded stream buffer. Speech/model failure cannot
stop USB capture. Microphone initialization occurs before Wi-Fi/BLE allocations;
the audio and speech task stacks are reserved statically. CPU-only HID JSON and
speech FIFO buffers use PSRAM, leaving internal RAM for DMA and speech models. Local failures report
specific reasons rather than describing all recognition failures as a missing
microphone. Attention tones wait until USB capture stops, preserving the
shared I2S bus while Desktop records. The existing GPIO map, codec configuration,
Wi-Fi/SSE behavior and partition boundaries remain unchanged.

## Desktop setup

The [official Micro guide](https://learn.chatgpt.com/docs/features/codex-micro)
explains that Micro's mic key uses the computer's selected microphone. Its default
is push-to-talk; a Voice Chat mapping is available when supported by the app.

1. Keep the device owned by Windows while testing Desktop (detach it from WSL
   after firmware work). Select **Microphone (TinyUSB UAC1)** as the operating system/app input
   microphone, and grant the app microphone access.
2. In Desktop's Codex Micro settings, map the Mic key to **Voice Chat** if you
   want a tap to start a voice chat. Otherwise, hold the board's **Micro voice**
   button for default push-to-talk and release it to stop. The guide also
   documents double-tap within 350 ms for hands-free dictation.
3. Watch for **Micro Linked**. Speak while the microphone is open: the board's
   waveform should change. Confirm sound using the host's input-level test or
   a recording before testing Desktop transcription.
4. **Wi-Fi voice** continues to use the original server, independent of Micro
   settings. USB streaming alone does not send a Wi-Fi message.

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
