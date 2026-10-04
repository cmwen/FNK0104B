# USB microphone diagnostic

`pio run -e codex-audio-diag` builds native CDC, Codex vendor HID and a UAC1
microphone at **16 kHz, mono, PCM16** using the same verified ES8311/I2S setup.
No Wi-Fi connection or speech recognition is required. It keeps the monitor's
partition boundaries; its upload does not rewrite the existing model image.

At 115200 baud, expect `monitor_audio ready=1`, microphone levels that change
with speech, and `usb_mic stream=on` when a host opens the microphone. Select
`Codex Micro` as the host's input device. Send `p` and `r` over serial for ACT10
press/release events; no input is sent automatically.

Building alone does not replace the running monitor. If uploading this
standalone diagnostic, build and restore `codex-monitor` afterward.
See [voice setup and runtime checks](../../docs/codex-audio.md).
