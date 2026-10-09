# USB microphone diagnostic

`pio run -e codex-audio-diag` builds native CDC, Codex vendor HID and a UAC1
microphone at **16 kHz, mono, PCM16** using the same verified ES8311/I2S setup.
No Wi-Fi connection or speech recognition is required. It keeps the monitor's
partition boundaries; its upload does not rewrite the existing model image.

At 115200 baud, expect `monitor_audio ready=1`, microphone levels that change
with speech, and `usb_mic stream=on` when a host opens the microphone. Select
`Codex Micro` as the host's input device. Send `p` and `r` over serial for ACT10
press/release events; no input is sent automatically. Send `m` to mute USB
output and `u` to unmute. During a continuous host recording, speech should
become silence while muted, then return without reopening the audio stream.
The diagnostic prints `audio_diag usb_muted=1/0`. Local sampling continues.

Building alone does not replace the running monitor. If uploading this
standalone diagnostic, build and restore `codex-monitor` afterward.
See [voice setup and runtime checks](../../docs/codex-audio.md).

Send `q` and `s` to press/release ACT11, the second microphone switch. Enable
separate microphone keys and assign the second switch to Voice Chat in Desktop
if supported. This diagnostic keeps raw capture available for low-level tests;
it does not enforce the monitor's default hold/release audio gate.
