# Usage reset countdown (monitor 0.8.4)

When the Wi-Fi bridge reports 100% used for the five-hour or weekly Codex
allowance, the monitor replaces the central agent tiles and direction/command
shortcuts with a reset countdown. Both exhausted windows get their own timer.
The usage header and bottom voice/send controls remain visible. Hidden tiles
cannot receive taps or draw animations over the timer. Local command help and
message recording temporarily take priority, then the countdown returns.

The timer uses the bridge's `resets_at` Unix timestamp and status clock, advancing
locally while the screen is awake. Under one day it shows hours, minutes and
seconds; longer waits show days, hours and minutes. A missing reset timestamp or
clock shows **Reset time unavailable**. At zero it shows **Awaiting usage update**;
only a new usage report below 100% restores the normal view. It never assumes
that reaching zero proves access has returned. Disconnect clears quota state.

The countdown takes priority over idle photos/blank mode. It does not keep the
backlight awake: the existing screen timeout, quiet mode and touch refresh apply.
The countdown has no new settings.

## Connection and voice behavior

Usage already comes from the bridge's `account/rateLimits/read` and
`account/rateLimits/updated` integration. The existing contract selects exact
300-minute and 10,080-minute windows; missing windows remain unknown. This
change adds no speculative USB protocol method. USB/BLE Micro packets currently
carry slot lighting, not usage or reset times, so USB/BLE alone cannot show a
countdown. Micro controls can be replaced by the timer when the independent
Wi-Fi bridge is also supplying usage.

Exhausted included usage does not prove every action is blocked. Available
credits can allow continued work. Local “Hi ESP” screen/status commands run on
the board. USB dictation and Voice Chat send microphone audio and Desktop key
events; bridge voice uses a configured transcription service followed by Codex
submission. Receiving audio or transcribing does not establish that Codex can
run a turn. Voice Chat availability at the Codex limit is **UNKNOWN** in this
integration, so voice controls stay available rather than being disabled from
the percentage alone.

Official sources checked 17 October 2026:

- [App-server rate limits](https://learn.chatgpt.com/docs/app-server): used
  percentages, window lengths and Unix-second reset timestamps.
- [Pricing](https://learn.chatgpt.com/docs/pricing): credits can allow work after
  included limits are reached.
- [Codex Micro](https://learn.chatgpt.com/docs/features/codex-micro): Desktop
  microphone mappings; no established guarantee here about Voice Chat at limits.

## Validation and limits

Native coverage checks either/both exhausted windows, unknown usage/time,
hours/days formatting and the reset boundary without assuming recovery.
Host previews cover five-hour, weekly, both, unknown-time, expired and Micro
layouts using the shared firmware renderer. `pio test -e native` passed 41
tests. The `codex-monitor` PlatformIO build and Astro guide build also passed.
Physical appearance, touch behavior and live account-limit transitions
remain **UNKNOWN** until tested on the board. No physical installation was performed for this change.
