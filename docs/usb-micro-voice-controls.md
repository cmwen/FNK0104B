# USB status and separate voice controls — 2026-10-10

Monitor 0.6.3 defaults to **Hold to talk / Voice (setup required) / Send**.
Outgoing board audio starts off. Hold to talk opens it while pressed and closes
it on release. The optional Voice button toggles continuous outgoing audio.
This supersedes 0.6.2's X mute button and unmuted boot default.

## Desktop configuration is a separate step

The [official Codex Micro guide](https://learn.chatgpt.com/docs/features/codex-micro)
documents Push to talk as the default microphone action and **Use separate
microphone keys** for independent switch mappings. It also documents Voice Chat,
if offered: tap to start or toggle its microphone, hold to end the chat.

The browser cannot change or inspect Desktop's mappings: no verified
configuration RPC or recording/session feedback is available. Keep the first
microphone switch on **Push to talk**. For the optional Voice button, enable
separate keys and assign the second switch to **Voice Chat**, if offered. Then
save **Hold to talk + Voice toggle** on the browser setup page.

Hold to talk forwards ACT10 down/up. Voice forwards ACT11 down/up. The
[retail capture](https://github.com/arthurcolle/codex-micro-open/blob/3ea3db39c85ca3240e9bee8d1bb0551bf34b1e63/reports/technical-dossier.md#41-physical-control-layout)
identifies ACT10 and ACT11 as the switches under the wide microphone cap. This
verifies the reference key identity, not acceptance on this board's host version.
The twelve-switch fttawa implementation exposes ACT10 alone, so it is not
used as evidence for ACT11. No external firmware or GPIO mapping is copied.

Select **Microphone (TinyUSB UAC1)** as Desktop's input and allow microphone
access. Review dictation, then press Send. There is no automatic sending:
verified transcription-ready feedback is unavailable. Desktop's double-tap
hands-free dictation does not bypass the board's strict hold/release gate.

## Board audio gate and indicators

- **Hold to talk:** opens audio on press; closes it immediately on physical
  release, even if the HID release event needs retry.
- **Voice, when enabled:** opens audio after a short tap; an off press closes
  audio immediately. A hold never briefly reopens previously closed audio.
  Tap again to close audio and request a host microphone toggle. A one-second
  hold also closes board audio, while the unchanged held key requests Desktop's
  hold-to-end behavior. The local threshold does not prove Desktop ended a chat.
- **Mutual exclusion:** Hold to talk is blocked while Voice audio is open.
- **Save, reconnect, boot or failed key enqueue:** closes the gate. Preferences
  persist; an open gate never persists across reconnect/reboot.
- **Mic ON:** live board audio is enabled, input is ready and a host USB stream
  is open. **Mic OFF:** outgoing board audio is silenced. **Waiting:** the gate
  is requested open, but no host stream is open. **No input:** capture is not ready.

The raw microphone still samples locally. A closed gate clears queued audio and
substitutes PCM silence to keep an open host stream healthy. These indicators
refer to board USB output, not physical sensor power, another host microphone,
confirmed recording, or confirmed desktop voice-session state. Closing audio
may leave the desktop voice chat open. Use Desktop's end action or hold Voice.
Wi-Fi wake/bridge recording retain their existing behavior; USB mode pauses
local recognition. The audio heartbeat now includes `usb_muted=0/1`.

## Settings protocol

The same BLE settings service gains characteristic
`4e4b0104-0004-4d20-8f4b-0104b0000001`, reading version 3 as six bytes:
`[3, volume, timeout_low, timeout_high, slots, separate_voice]`.
Volume is 0–100, timeout 1–120 minutes, slots 3 or 6, and separate_voice 0 or 1.
The preference is saved as `monitor/usbVoice`, default false. The main loop
applies changes and closes audio without reboot.

The browser tries v3, then v2, then v1 characteristics. The older characteristics
retain their exact read formats. Legacy writes preserve the new voice preference;
invalid writes change nothing. Voice selection is disabled on older firmware.
The settings service reserves 24 handles for the added characteristic.

The page presents two steps side by side on desktop, stacked on mobile:
**choose board controls** and **map the keys in Desktop**. It explicitly says
saving the browser form changes only the board. Live gate semantics and gesture
limitations are in an expandable explanation.

## Status disappearing (retained from 0.6.2)

Code inspection found receive overflow advanced the USB connection epoch,
clearing discovery and all six lighting slots. This is a plausible cause of the
reported simultaneous dropout, not a physical reproduction. Overflow now
advances only the fragment generation, resetting decoding at the gap while
retaining discovery, queued releases and last valid lighting. Actual USB
start/stop still resets the session. RX drains during transmission, button
input takes precedence over background ACKs, and microphone gestures redraw
only the footer. Lighting remains host-supplied and does not establish identities.

## Validation and remaining checks

29 native tests pass, including default closed audio, hold/release, separate
Voice toggle, mutual exclusion, long holds, millis wrap, disconnect reset,
ACT11 encoding, v3 validation and legacy preference preservation. Eight browser
protocol tests and two generated-site tests pass. Browser smoke checks assert
field order, contained controls and no horizontal overflow at 1440, 768 and
390 pixels. Host previews use the shared 320×240 firmware renderer.

The named `codex-monitor` and `codex-audio-diag` PlatformIO builds passed.
No pin, partition, toolchain or security settings are changed. The initial implementation session had no visible device. Following user
authorization, 0.6.3 was uploaded with hash verification; 115200-baud serial
confirmed startup muting and live Desktop status delivery. See
[device evidence](../test/hardware/usb-micro-voice-2026-10-10.md). Physical acceptance remains **UNKNOWN**: Desktop ACT11 and
separate mapping acceptance, hold dictation and transcription, Voice start/mute/
end, real PCM silence/resume, settings persistence and BLE reconnect, and the
original simultaneous status dropout. Audio diagnostic commands `p/r` exercise
ACT10, `q/s` ACT11, and `m/u` shared USB mute/unmute. Its raw-capture mode is
intentionally independent of the monitor's default audio gate.
