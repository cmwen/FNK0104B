# USB Micro hold-to-talk and Voice controls — 2026-10-10

Monitor 0.6.3 was built and uploaded with PlatformIO Core CLI to the known
FNK0104B, serial B81F3FC39F94. Firmware SHA256:
`15fb3dc84bcfc169f9cb79704d8fab13c8699c031e02555e3a977504b1a2832c`.

The first upload entered ROM download mode but timed out during USB enumeration.
A retry using the normal repository configuration succeeded, verified all image
hashes and issued the configured watchdog reset. No temporary configuration was
used. See [upload log](usb-micro-voice-2026-10-10/upload.log).
The existing bootloader, partition table, app and speech models were written at
their existing offsets; no partition boundaries, GPIOs, stored credentials,
eFuses or security settings were changed and no erase-all was used.

Windows subsequently enumerated USB 303a:8360 with HID, COM9 and TinyUSB UAC1.
A temporary localhost COM9 bridge allowed PlatformIO serial monitoring at
115200 baud while Windows retained HID/audio. The bridge closed after capture;
no permanent service was installed. See
[runtime log](usb-micro-voice-2026-10-10/runtime.log).

The short capture showed microphone `ready=1`, `reason=none`, `usb_muted=1`,
`usb_stream=0`, `usb_dropped=0` and heap 44095. Desktop USB status methods were
decoded, replies completed and status revisions 5 and 6 reached the display.
No panic or audio error was observed in this capture. This verifies startup
muting and live status delivery, not sustained recovery or gesture acceptance.

Physical hold/release dictation, transcription, ACT11 and separate Desktop
mapping, Voice Chat start/mute/end, actual PCM silence/resume, BLE preference
persistence and the original simultaneous agent-status dropout remain UNKNOWN.
The owner will test these interactions. Software checks passed: 29 native tests,
eight browser protocol tests, two generated-site tests, responsive browser smoke
checks and both affected firmware builds.
