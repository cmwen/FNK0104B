# Connectivity implementation validation — 2026-10-07

This is software/build evidence for the connectivity revision (monitor 0.6.0),
and subsequent physical checks. The firmware was flashed successfully after
the user resumed testing. Sandboxed device discovery hid an attached board;
escalated PlatformIO discovery found it. See the
[device record](../test/hardware/connectivity-modes-2026-10-07.md) for upload
hashes, live USB status replies, five-second host PCM recording and remaining
checks. Earlier hardware records remain historical.

## Automated checks

- PlatformIO native: 26 tests passed, including six Micro agent IDs, all command
  press/release encodings, rejected invalid IDs, touch gaps, enumeration versus
  discovery, USB precedence and quiet-session retention.
- Bridge unittest suite: 65 tests passed. BLE voice enters intake with no agent;
  direct targets, duplicate routes and unknown routes are rejected. Missing
  intake returns 503 rather than creating a thread. Authentication remains required.
- Shared-theme host previews: six tiles fit the 320×240 viewport with Mic/Send
  and a separate command grid. This does not prove real touch comfort or LCD rendering.
- PlatformIO firmware builds passed: `codex-monitor`, `codex-ble-diag`,
  `codex-audio-diag`, `codex-hid-diag`, `speech-diag`, and `recorder`.

Build logs used during the session are in `/tmp/fnk-connectivity-final-builds.log`,
`/tmp/fnk-connectivity-native-final.log` and
`/tmp/fnk-connectivity-bridge-final.log`; they are local temporary artifacts.
The pinned toolchain, partition layout and GPIO assignments are unchanged.
Final monitor/HID edits are checked separately in
`/tmp/fnk-connectivity-final-rebuild.log` and
`/tmp/fnk-connectivity-monitor-final.log`.

## Physical acceptance still required

1. Build/upload `codex-monitor` using PlatformIO, then inspect serial at 115200.
   Give USB ownership to the Desktop host for Micro/audio testing.
2. Adapter power: bridge UI, Hi ESP, commands and existing Wi-Fi voice work.
   Host USB enumeration without Micro discovery keeps the bridge experience.
3. Desktop discovery: six assigned slots, raw color/brightness updates, remapped
   keys, foreground/focus, taps/double taps, Send and Mic press/release work.
   Approve/Reject are Micro-only keys for Desktop's current request.
4. Select UAC1 as the host input. Record audible 16 kHz mono PCM and test
   Desktop dictation. Confirm speech logs pause before USB samples are allowed;
   local wake/commands do not run in USB Micro. Repeat transitions without
   stale audio, stuck keys, crashes or allocation failures.
5. Cold boot without Wi-Fi credentials: automatic setup yields on USB discovery
   via one reboot; explicit setup remains open. Quiet Micro stays connected.
   USB disconnect on this batteryless device means power loss. App-exit detection
   while USB remains mounted is UNKNOWN.
6. Lower priority BLE: first run `codex-ble-diag`; its initial pairing window is
   60 seconds. Check encrypted report 6, Input/Output/Feature, MTU >= 66,
   notification subscription, bonding, reconnect and actual Desktop discovery.
   The current report map includes only the vendor collection; retail keyboard,
   consumer and mouse collections are not claimed. Descriptor parity is UNKNOWN.
7. Integrated BLE: hold the Micro/Codex header three seconds to pair. Select
   different Micro slots and speak via Hi ESP/Orchestrator. Confirm Wi-Fi STT
   always enters orchestrator intake. Test missing intake and Wi-Fi failures,
   and USB preference when both sessions exist. Recheck Web BLE settings.

No measured battery percentage or semantic interpretation of Desktop slot
colors was added. HID transmission retains failed frames and paired releases
until acceptance or connection reset; congestion, overflow and host disconnect
cleanup still require physical stress testing.
