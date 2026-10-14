# Rear status LED

Monitor 0.8.2 settings now includes Rear status LED (enabled by default) and Status
LED timeout (60 seconds by default; configurable from 5 to 600 seconds).
Preferences apply without reboot and persist in NVS. Turning the indicator off
clears it on the next main-loop tick. Re-enabling waits for the next meaningful
status change. Old settings clients preserve these preferences.

Green means working, red means attention/error, and yellow means observed work
has ended. Attention takes priority over working when several agents are active.
Startup, disconnection, provisioning without a host, and firmware updates leave
it off. Each state/active-agent identity change starts a bounded notification;
heartbeats, brightness and animation updates do not extend it. The timeout also
applies while an agent remains working or needs attention.

USB uses the existing retained slot state: blue working, orange attention and
red error. An active-to-known-white/green-idle transition produces yellow.
Black/blank host updates remain ignored under the existing LCD retention policy;
if the host supplies only black on completion, yellow cannot be inferred.
Unknown host colors do not generate a completion notification. Wi-Fi uses bridge
agent IDs/states; a connected snapshot changing from work/attention to no active
agents produces yellow. This indicates work ended, not a verified successful
result: the current bridge does not distinguish completion from cancellation or
removal. Disconnects and unavailable/degraded integration do not produce yellow.

Board access lives in `lib/fnk0104b`; GPIO42, one pixel and GRB order follow the
[official Freenove LedPixel example](https://github.com/Freenove/Freenove_ESP32_S3_Display/blob/main/Tutorial_With_Touch/Sketches/Sketch_02.1_LedPixel/Sketch_02.1_LedPixel.ino).
The existing Arduino 3.3.12 component supplies the RMT-based RGB writer. No pin,
partition, toolchain or security setting changes are required. The isolated
`rgb-diag` PlatformIO environment cycles green/yellow/red/off before physical
integration acceptance. The owner confirmed the diagnostic green/yellow/red/off colors on 2026-10-14.
The restored monitor serial check verified the default 60-second shutoff
during repeated working updates. Physical BLE persistence, custom timeout
acceptance and integrated attention/completion colors remain UNKNOWN.

BLE adds settings characteristic `4e4b0104-0009-4d20-8f4b-0104b0000001`.
Version 5 is ten bytes: version, volume, screen timeout LE16, slots, separate
voice, avatars, LED enabled, LED timeout seconds LE16. Versions 1–4 and their
existing characteristics remain unchanged. Invalid packets change nothing.
