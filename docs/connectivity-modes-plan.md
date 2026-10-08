# Connectivity modes plan

**2026-10-08 update:** The monitor now reserves BLE for browser configuration and defaults USB Micro to three agent slots plus four joystick controls. See [the decision and implementation](usb-micro-controls.md); earlier BLE/six-slot sections below are historical.


Revised for owner clarification, 2026-10-07. Planning only; no firmware or
bridge behavior changed. This revision supersedes the initial manual mode
selector, computer-microphone BLE path, proposed Wi-Fi approval extension, and
BLE slot-to-thread speech targeting. USB HID and the existing Wi-Fi bridge
experience are the first release priorities; BLE support is a later milestone.

## Agreed experience

The device automatically presents one of three experiences according to a
verified Desktop Micro connection. Wi-Fi remains available for bridge work.

| Experience | Agent display/control | Board microphone | Local wake/recognition |
| --- | --- | --- | --- |
| Bridge, no Micro connection | Existing bridge agents and targeting | Existing bridge speech-to-text | Keep Hi ESP and commands |
| BLE Micro connected | Six Desktop-configured Micro slots and mapped keys | Wi-Fi audio upload → bridge speech-to-text → orchestrator | Keep Hi ESP and commands |
| USB Micro connected | Six Desktop-configured Micro slots and mapped keys | USB UAC1 input selected by the host | Disable wake, commands, AFE, local VAD and bridge recording |

Approvals are **Micro-only and low priority**. Keep existing bridge approval
blocking; no approval endpoint or spoken approval command is planned.

The owner has one USB connector and no battery. Every boot starts USB discovery
support because the cable may connect to a computer. A power adapter/cable,
USB enumeration, or ordinary serial use alone does not select Micro. BLE Micro
can operate while USB supplies power. Without any USB power, the present device
is off; "fully wireless" currently means wireless data with external USB power,
not battery operation. No battery detection or battery telemetry is assumed.

## Automatic discovery and connection policy

1. Initialize the existing composite USB transport and discovery responder at
   boot. Preserve CDC recovery and the current partition layout. In the later
   BLE milestone, advertise BLE HID for pairing/reconnection using the same
   BLE stack as existing services. Existing BLE settings/provisioning remain.
2. Show Bridge while waiting for a verified Micro session. Respond promptly to
   Desktop discovery/status calls and complete their replies before reporting
   Micro connected. Discovery is host-led in the observed protocol: "initiate
   handshake" means make the device discoverable/respond correctly, not invent
   an unsolicited device-to-host RPC.
3. Select USB Micro when a USB Micro session is established; otherwise select
   BLE Micro when a BLE Micro session is established; otherwise use Bridge.
   USB priority when both sessions exist is a proposed implementation default.
   BLE may remain bonded, but only one Micro transport emits controls at once.
4. A quiet valid host is not disconnected merely because it sends no lighting
   updates. Retain Micro Idle while the session remains valid. USB unmount or
   BLE disconnect clears that session and selects another established session,
   or Bridge. Validate a host-session health mechanism before treating app
   exit, sleep or a half-open connection as a disconnect. Current HID traffic
   has no authenticated desktop identity or definitive app-exit notification.
5. On every transition, release held controls on the old connected transport,
   clear queued inputs and stale slot/selection state, and advance a connection
   epoch. Never replay presses, messages or approvals on the new transport.
   Finish an already-submitted bridge job on its original target; do not move
   it. Cancel unsent capture before activating a new audio policy.

No reboot is required for normal automatic discovery or fallback. Persist BLE
bonds and user preferences, not a stale "Micro connected" flag. Bound discovery
work so an absent Desktop cannot delay the current bridge indefinitely.

The current worker can only infer discovery from recognized incoming calls.
Strengthen it with completed-reply/session evidence. A compatible probe can
still imitate Desktop; label this as protocol connectivity, not authentication.
Exact app-exit/health behavior remains an acceptance gate, not an invented
one-minute timeout. Wi-Fi failure does not demote a BLE Micro session: retain
Micro agents/keys and show bridge speech unavailable independently.

## Six agents on the screen

Try all six assignments in a **3-column by 2-row grid** on the 320×240 display.
Reserve about 36 pixels for the header and 56 for voice/command controls, leaving
about 148 pixels for two tile rows. Each tile shows slot number, compact avatar,
status color and selection marker; titles appear only if received through a
verified host interface. Idle/unassigned slots retain stable positions.

Keep the full six-slot protocol model and AG00–AG05 mappings. Do not sort slots
by bridge activity: Desktop configuration owns their meaning. If physical touch
or readability is poor, retain all six using two pages of three, with a visible
page indicator. The current bridge UI/catalog remains independent.

Desktop owns key assignments, single/double-tap focus policy and microphone key
behavior. Device controls emit verified IDs and press/release edges. Capture
actual desktop configuration/status exchanges before translating colors/effects
into semantic states or exposing new command buttons. ACK-only configuration
methods today do not prove that remapping works end to end.

## BLE speech: bridge orchestrator intake

BLE HID carries Desktop slot selection/control and status. Speech is a separate
path: board microphone → Wi-Fi → bridge transcription → existing orchestrator.
Every BLE speech submission enters the orchestrator; it never directly targets
the selected Micro agent. The orchestrator owns routing and clarification.

Do not attach a Micro slot, bridge-selected agent ID, or cached thread target to
BLE voice requests. Desktop slot selection/remapping cannot change voice intake.
No slot-to-thread lookup, assignment synchronization or composer insertion is
needed. Verify the existing untargeted voice path invokes the orchestrator;
if its configuration is unavailable, report that rather than silently using a
direct-agent/new-thread fallback.

Do not emit the Micro Mic key for this bridge recording and accidentally start
a second host recording. Label the control as orchestrator speech so the user
can distinguish it from desktop-mapped Micro actions. Keep Hi ESP and local
commands; pause recognition during capture/submission. Show Wi-Fi/transcription
failure independently of BLE state and retain Micro agent controls on failure.
Preserve existing Bridge-mode routing behavior; this rule specifically governs
BLE speech. USB Micro continues to use the host's USB audio/voice path.

## USB microphone and dynamic speech lifecycle

Keep one board-owned ES8311/I2S raw capture path. USB Micro uses the existing
PCM16, 16 kHz mono UAC1 input. The host selects the board microphone and owns
recording duration, dictation and Voice Chat. No local wake, command inference,
AFE, VAD or bridge recording runs in USB Micro. Defer speaker tones during host
capture. HID discovery alone does not prove microphone PCM or transcription.

Automatic USB discovery may happen after local recognition has started. Add a
safe quiesce/resume lifecycle: stop speech input subscription and inference,
clear captured samples and pending command events, then enable the USB-only
path. Models may remain allocated but dormant initially if this prevents
fragmentation; no local inference/feed tasks should continue doing audio work.
On fallback to BLE/Bridge, resume/reinitialize recognition cleanly with no
stale command or audio. Current speech code has no such lifecycle and must be
extended before automatic switching is complete. Validate heap over repeated
transitions; USB audio must remain usable even if recognition startup fails.

Keep existing USB descriptors and CDC enumeration across experiences. Gate
live USB PCM by established USB Micro session and host stream enable. A powered
USB connection in Bridge/BLE must not export board audio unintentionally. The
host may still list the USB microphone interface while inactive; hiding the
interface would require a separate re-enumeration design.

## Reverse-engineering references and feasibility

Existing research is recorded in [codex-hid.md](codex-hid.md), including pinned
source snapshots. Rechecked reference material during this planning revision:

- [codex-micro-open technical dossier](https://github.com/arthurcolle/codex-micro-open/blob/main/reports/technical-dossier.md)
  documents USB/BLE vendor report 6, framing, discovery/status, six agent IDs and
  command input edges. Its host-originated device.status round trip is a health
  check; this is not an unsolicited firmware-initiated handshake.
- [Captured BLE validation](https://github.com/arthurcolle/codex-micro-open/blob/main/evidence/live/2026-07-31/ble-validation.json)
  reports a matched status reply on retail hardware over macOS BLE. Its BLE
  descriptor includes a 63-byte report 6 Feature item and omits USB gamepad
  report 4. Do not assume the USB descriptor can be copied unchanged for BLE.
- [fttawa/codex-micro](https://github.com/fttawa/codex-micro)
  supplies another unofficial compact RPC/input interoperability reference.
- [codex-island-esp32s3](https://github.com/lxw666598/codex-island-esp32s3)
  reports USB/BLE vendor HID on another ESP32-S3 board and macOS. Its hardware,
  power policy and firmware build workflow are not applicable to FNK0104B.

These are primary observations/implementations by third parties, not proof on
our board or the owner's installed Windows Desktop. Reuse verified protocol
facts; do not import board mappings, power assumptions or an ESP-IDF workflow.
Record the exact reference commits and any adapted code attribution during
implementation. Start with a standalone BLE HID diagnostic and test report map,
GATT Input/Output/Feature behavior, report-ID handling, notification subscription,
MTU/packetization, bonding, reconnection and actual Desktop discovery.

Existing USB Micro discovery and UAC1 enumeration have historical board evidence.
Host PCM, touch inputs, slot status semantics and BLE compatibility remain open
checks in [codex-audio.md](codex-audio.md) and
[monitor readiness](monitor-readiness.md). Battery state remains UNKNOWN; current
USB compatibility placeholders must not be displayed as measured battery data.

## Implementation order and acceptance

Complete and validate the USB/Wi-Fi milestone before adding BLE HID. Low-priority
BLE and approvals do not block delivery of the primary experiences.

1. Generalize USB input IDs to six stable agent controls; observe Desktop
   remapping/focus/status, and validate host microphone recording. Prototype the
   six-tile layout with actual touch targets. Use Windows first based on current
   evidence unless the owner specifies another desktop host.
2. Add/test automatic connection policy and speech quiesce/resume. Verify USB
   power without Micro keeps Bridge; late discovery enters USB Micro; no local
   speech processing runs there; Desktop loss/fallback restores bridge speech.
3. Validate the primary USB/Wi-Fi milestone: six Desktop assignments, key
   mapping/focus, USB PCM and Desktop speech; adapter-powered bridge cold boot,
   existing orchestrator voice, Hi ESP, Wi-Fi recovery, late USB discovery and
   fallback. Test held-key cleanup and repeated speech lifecycle transitions.
   With no battery, unplugging USB is a power-loss/cold-boot test, not
   continuous-operation transport failover.
4. Later, add a small named PlatformIO BLE Micro diagnostic, then integrate BLE HID
   with settings/provisioning on the existing stack. Bonding alone is not Micro
   discovery. Verify BLE Micro while USB supplies power and USB discovery is absent.
5. Add BLE Wi-Fi orchestrator speech. Verify voice requests always use intake
   without direct-agent targeting, including after slot selection/remapping and
   transitions from a targeted Bridge view. Test clarification, disconnects,
   Wi-Fi/transcription failures, both connections present and USB preference.
6. Add Micro-only Approve/Reject last using verified desktop command IDs and
   mappings. Show their desktop-current-request scope; no bridge approval path,
   voice approval, automatic approval or replay after reconnect.

Keep hardware access in lib/fnk0104b and hardware-independent protocol/policy in
shared libraries. Use PlatformIO Core CLI named environments, build before any
upload, inspect serial at 115200, and retain the pinned toolchain and partition
layout. Run native tests for protocol, connection transitions and stale-event
cleanup; bridge tests for orchestrator intake changes. Test hardware with USB owned by the
Desktop host rather than WSL USB/IP, then return ownership for PlatformIO work.
Update dated evidence and knowledge only after physical verification. No erase,
new GPIO assignment or security-setting changes are needed for this plan.

If BLE Micro discovery cannot be proved, report that specific gap and preserve
the working bridge/USB paths rather than claiming parity. BLE speech has no
dependency on discovering the Desktop's slot-to-thread assignments.

## Implementation status, 2026-10-07

The software implementation is present: automatic USB-first discovery policy,
six Micro slots and command keys, speech pause/resume with raw USB capture,
BLE vendor HID diagnostic/integration, and enforced BLE orchestrator voice
routing. Native and bridge tests pass. Firmware builds and remaining physical
acceptance checks are recorded in
[connectivity validation](connectivity-modes-validation.md). Hardware acceptance
remains pending; this status does not establish Desktop interoperability.

Reference heads inspected during this implementation (protocol facts only; no
external board or firmware source was imported):

- codex-micro-open: `3ea3db39c85ca3240e9bee8d1bb0551bf34b1e63`
- fttawa/codex-micro: `80d20ba1eec66071261505e089e0851cd91761eb`
- codex-island-esp32s3: `0116fb506da813102651d78467cddb607fbdcb82`
