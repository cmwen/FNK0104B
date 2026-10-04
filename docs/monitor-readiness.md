# Codex monitor readiness

## Luna dispatcher implemented, 2026-10-04

The bridge now queues new text/voice commands, uses GPT-6 Luna to select from
an environment-configured repository root, validates the choice on the host and
starts the coding task in that repository. Ambiguous commands receive a question;
explicit existing-agent replies retain their thread. Dispatcher stages and brief
completion results use the existing SSE avatar format. The running LocalLink
service is configured for `/home/cmwen/dev` with 78 discovered repositories,
Luna dispatch and GPT-6.1 Sol coding. All 60 host tests passed; real task and
clarification dispatch completed. PlatformIO built/uploaded the board's queued
acknowledgement and request-ID update with hash verification and unchanged
partition boundaries. At 115200 baud, serial confirmed network reconnection and
live dispatcher status after a normal USB reset. Current physical voice submission
remains **UNKNOWN**. See the [implementation record](../test/hardware/codex-monitor-dispatcher-2026-10-04.md)
for the initial AFE warning/recovery, evidence and remaining limits.

## Local orchestrator research, 2026-10-04

The [orchestrator feasibility record](local-orchestrator-research.md) includes
real GPT-6 Luna structured routing and ambiguity probes, a repository-specific
coding handoff, and a completed task through the running bridge's text endpoint.
All 48 bridge tests passed. PlatformIO serial at 115200 confirmed live SSE agent
count/quota updates on the attached board. No voice capture/upload was observed
during this check. Production repository discovery/routing is not implemented;
the current physical voice-to-routed-task path remains **UNKNOWN**. The record
lists proposed environment settings, latency/clarification/retry gaps and the
acceptance checks needed before declaring that path ready.

Reviewed 2026-09-29. The integrated monitor has now been uploaded and exercised against a controllable LAN mock. This verifies board polling and status transitions; it does not establish live Codex agent visibility.

## SSE firmware uploaded, 2026-10-03

The `codex-monitor` 0.4.0 firmware now receives an authenticated SSE stream
instead of polling status every five seconds. Inactivity closes that socket,
turns the backlight off, and blocks the status task without reconnect attempts.
Touch consumes the wake gesture, clears old display values and reconnects with
`refresh=1`. Awake connections block in `select()` between arrivals and use
5–60-second retry backoff after failures. Wi-Fi association, BLE and touch
scanning stay enabled. The existing screen timeout (default 30 minutes, BLE
setting 1–120 minutes) and active-agent/voice keep-awake rules still apply.

All 48 Python tests and 15 native tests passed. PlatformIO Core built and
uploaded the firmware to `/dev/ttyACM0` with hash verification: RAM 89,844 bytes,
flash 1,445,161 bytes (45.9% of the existing app slot). No partition-layout
change or erase-all was performed. Serial at 115200 confirmed stream
connection, status delivery, and recovery after restarting the bridge.

A 60-second idle observation with the board connected recorded zero Codex
requests and one 13-byte SSE heartbeat. Codex daemon CPU averaged 0.033%;
bridge CPU averaged 0.017%. The daemon still reports persisted-only thread
visibility, so the board correctly shows degraded integration.

During the owner's subsequent short BLE-timeout check, serial confirmed
quiet-mode entry, stream disconnection with reason `quiet`, touch-wake,
reconnection and fresh quota delivery. Host logs independently recorded the
stream closing, reopening and sending a new status frame. No voice capture
was observed from the wake touch. The exact elapsed timeout was not measured.
Active/voice keep-awake behavior with SSE and board energy savings remain
**UNKNOWN**. Host tests verify cancellation of the subscription and fallback
refreshes when the client closes its socket. See the
[SSE hardware record](../test/hardware/codex-monitor-sse-2026-10-03.md).

## Earlier quiet-mode build before SSE, 2026-10-03

The `codex-monitor` 0.3.0 build uses the existing idle screen timeout to pause
status polling when the backlight turns off. Touch wakes the screen, consumes
the wake gesture, clears stale display status, and requests fresh agents and
quotas through the authenticated `refresh=1` status query. Active agents and
voice work retain their existing keep-awake behavior. New-agent alerts wait
until touch-wake while quiet. Wi-Fi association, BLE and touch scanning remain
available; no ESP32 deep-sleep configuration was added.

`pio run -e codex-monitor` passed, all 13 native tests passed, and all 39 bridge
tests passed. The live bridge was restarted and its authenticated refresh
endpoint returned HTTP 200, cache age zero and a quota observation timestamp.
The integration remained degraded for the existing persisted-only thread
visibility reason. `pio device list` returned no serial devices, so no upload
or physical sleep/wake check was performed. Device behavior and energy savings
are **UNKNOWN** until tested; that build was not uploaded before the SSE update below.

After uploading through PlatformIO, set the existing **Idle screen timeout** to
one minute for a short check. With no agents or voice activity, inspect serial
at 115200 for `monitor_sleep state=quiet polling=paused` and confirm that board
requests cease after any in-flight request finishes. Touch once and expect
`monitor_sleep state=awake reason=touch`, a fresh HTTP status request, and
resumed polling; that touch must not start voice capture. Also exercise active
agents/voice keeping it awake and network recovery after touch-wake. Restore
the preferred timeout afterward.

## Live host service check, 2026-10-02

The real bridge was started on `192.168.1.32:8765` using the host, port and key
from the ignored device configuration through `monitor-server/run.py`.
An authenticated HTTP status probe returned `connected`, one visible active
agent, and both exact quota buckets; an unauthenticated probe returned HTTP 401.
All 20 host tests passed. The earlier persisted-only thread visibility result
below remains a historical observation; this connection now reports an active
agent. Visibility of every separately running Codex client is still unverified.
No board-originated request or serial device was observed during setup, so this
check establishes host readiness, not a live board-to-bridge exchange.

The running bridge was subsequently configured for the existing local speech
facade at `http://127.0.0.1:8790/v1/audio/transcriptions` in multipart mode.
The bridge's WAV client completed a generated one-second silent-audio request;
the facade returned no transcript, which the bridge correctly rejected without
issuing a Codex command. This verifies endpoint and request-format compatibility,
not spoken-word accuracy. The facade's overall health reports missing synthesis
and Chinese-language models; its default English STT model was not listed as
missing. A real spoken sample and board voice upload remain open.

After USB attachment, the board at `192.168.1.159` repeatedly polled the real
bridge and received HTTP 200 with `integration: connected`. The host text
command endpoint created a test Codex thread whose turn completed with the
requested `monitor test successful.` reply. The subsequent board `/v1/voice`
upload returned HTTP 200 and created a Codex thread that completed without error.
Its recognized text was incomplete (`to test successful. Do not change any`),
so Codex asked what to test. The full voice transport works; spoken-instruction
accuracy and targeted replies still need checking. See
[the live test record](../test/hardware/codex-monitor-live-2026-10-02.md)
for evidence and the remaining physical checks.

## Responsiveness and usage update, 2026-10-02

After the owner reported missed voice-button taps and hidden usage, firmware
was changed to poll HTTP in a separate task, keep touch and display state on
the main task, show five-hour/weekly usage in the header during agent activity,
and distinguish microphone preparation from sending. It now displays recognized
text briefly after submission and logs recording duration and HTTP result
without logging transcripts. The `codex-monitor` build passed (1,434,309 bytes,
45.6% of the app slot), and all 20 host bridge tests passed. The initial MCP
upload was held by its firmware-write approval policy.

A follow-up investigation reproduced lost quota keys when ArduinoJson's
mutable-input zero-copy document outlived its HTTP buffer. Firmware now passes
const input so the document owns its strings before the buffer is freed. A host
C++ check using the pinned ArduinoJson headers reproduced the old behavior and
verified that both quota fields survive buffer reuse with const input. The live
bridge returned valid values (13% five-hour, 86% weekly) during this check;
unknown percentages on the board are not evidence of missing host quotas.
At the owner's explicit request, the updated firmware was subsequently flashed
directly with `pio run -e codex-monitor -t upload --upload-port /dev/ttyACM0`.
The upload succeeded with hash verification and retained the existing partition
layout. The board resumed real bridge polling. A 115200-baud serial inspection
and startup capture produced no new firmware lines; numeric quota appearance
and improved touch responsiveness still require the owner's visual check.

The owner then reported a red bridge icon and continued unknown quotas.
Diagnostic firmware showed repeated `Status HTTP -11` (read timeout) over serial
and on screen. A timed real bridge request took 3.07 seconds, exceeding the
former 2.5-second firmware timeout. The status timeout was increased to 15
seconds with a separate 5-second connection timeout, while polling stays off
the UI task. Direct CLI build/upload succeeded with hash verification, and
115200-baud serial recorded:

```text
monitor_status integration=connected agents=1 first_state=running five_hour_used=24 weekly_used=88
```

Live quota parsing on the updated board is now verified. Touch responsiveness
and the updated voice-stage display still require a physical interaction check.

At the owner's request, quota cards, bars and the persistent header now show
remaining percentages (`100 - used_percent`) with `left` labels. Unknown values
remain unknown. The updated CLI build/upload passed with hash verification;
the HTTP payload and serial status retain their used-percent meaning.

## Idle screen timeout, 2026-10-02

Screen sleep now requires a continuous idle interval with no reported active
agents and no voice preparation, recording or submission. Work arriving while
asleep wakes the display. Touch resets the interval. The existing Web BLE/NVS
packet remains compatible; the web control now says **Idle screen timeout**
and retains 1–120 minutes, default 30. Native timing checks cover prolonged
activity, completion followed by a full idle interval, activity after sleep,
touch resets, changed timeout values and `millis()` rollover. Real timed sleep,
automatic wake and a browser BLE write still require physical verification. All 13 native tests, the firmware build and the Web BLE bundle build passed. The update was flashed through PlatformIO Core CLI with upload hash verification; no partition-layout change or erase-all was performed.

## BLE discovery correction, 2026-10-02

After the owner could open the settings page but could not discover the board,
source inspection found that the pinned BLE library copies the advertised
128-bit service UUID into its default scan response along with the full device
name and TX power. That combination exceeds the legacy packet budget. Monitor
firmware now explicitly puts flags and the full `FNK0104B-MONITOR` name in the
primary packet, and the service UUID alone in the scan response. No pairing
mode is required. CLI build/upload passed with hash verification. A normal USB
reset and startup capture at 115200 baud verified:

```text
monitor_ble advertising_data status=0
monitor_ble scan_response status=0
monitor_ble advertising_started status=0
```

These statuses verify controller acceptance and advertising start, not reception
by Windows. Browser discovery, settings read/write and reconnect still require
the owner's check.

## Implemented path

### Reference UI polish, 2026-10-02

The owner's reference B was adapted to the 320×240 screen: an angular cyan
status strip with reference-derived Wi-Fi/robot icons, actual RSSI bars, separate
Codex state, and segmented remaining-quota bars. The idle screen has two quota
cards and a full-width microphone control; active agents retain animated avatars.
Quota values remain percentages, not inferred hours. Host previews using the
shared drawing helpers and pinned TFT font tables covered idle, active,
attention, offline, recording, 0% and 100%. The CLI build passed at 1,438,669
flash bytes and 89,820 RAM bytes, and USB upload completed with hash verification.
Post-upload serial at 115200 baud reported `integration=connected`, one running
agent, and used quotas of 39%/90% (61%/10% remaining). Physical readability and
touch placement still need an owner check.

| Capability | Current implementation | Evidence and limit |
|---|---|---|
| Display and touch | Landscape idle quota screen, larger animated active-agent avatars, tap selection, voice button, and display sleep | Owner confirmed the avatar floats and looks around without green flashing, and the larger connection icons are clear. Touch targets and 30-minute sleep/wake remain open. |
| Wi-Fi and Codex connection | Board polls a LAN bridge or mock; Wi-Fi and bridge state are separate indicators | Board polled the LAN mock from 192.168.1.149; serial recorded connected, degraded, unavailable, and recovery states. Live bridge-to-board path is not yet checked. |
| Codex status and limits | Python bridge reads app-server threads and exact 300-minute and 10,080-minute rate-limit buckets | A live HTTP probe retrieved both usage values and reported `degraded` with zero visible agents because the daemon listed this active conversation as `notLoaded`. Missing buckets are shown as unknown. |
| Voice command and reply | Board captures a short WAV; bridge transcribes on the host and creates or addresses a Codex thread | Microphone input was measured on this board. A speech endpoint and a live board-to-host voice run are still needed. The board does not authorize Codex approval requests by voice. |
| Attention alert | A newly attentive agent triggers a short speaker tone when playback is available | Board serial reported `monitor_attention_tone played` on the mock attention transition. Audible output remains unconfirmed; a speaker must be connected to the PH1.25 connector. |
| Web BLE settings | Separate monitor service stores notification volume and screen timeout; web page reads and writes the setting | Web build checks exist; browser-to-board settings have not been checked. Backlight turns off after the timeout; no intermediate brightness level has been verified. |
| Packaging | Independent `codex-monitor` PlatformIO environment and web flasher catalog entry | Animated landscape build passed at 45.6% of the 3 MiB app slot. USB upload hashes verified; status output was inspected at 115200 baud. |

The transport, payload, states, removal rule, and BLE packet are in [monitor-contract.md](monitor-contract.md). The local service and setup steps are in [the bridge README](../monitor-server/README.md) and [the firmware README](../apps/codex-monitor/README.md).

## Unknown or unverified

1. **Live transcription endpoint:** `TRANSCRIBE_URL` must point to a trusted local service. The LocalLink speech backend is a possible match if its endpoint is reachable and its request format is configured. No host endpoint was supplied or verified for this monitor.
2. **Remaining physical checks:** touch targets, sustained free memory, actual Wi-Fi drop/reconnect, voice capture and upload from the board, audible speaker tone, and browser-to-board BLE settings. Mocked bridge HTTP error/recovery and the animated landscape screen were checked. The speaker is optional hardware and has not been heard in a saved test.
3. **App-server agent visibility:** a read-only probe found the Codex daemon's thread records but marked this active conversation `notLoaded`. The bridge must be checked with a thread it starts itself, and an adapter or shared runtime may be needed to observe agents running in a separate Codex app process. Pending approval and user-input notifications depend on what the bridge connection receives. Approval remains in a Codex client.
4. **Display capacity and interaction:** mock overflow and 0%/100% quota payloads reached the board; the owner confirmed the main landscape avatar screen. Precise tile taps and overflow label remain unverified visually.
5. **Speaker connection:** the documented board wiring has an amplifier enable but no verified speaker-present signal. The firmware can play a tone when its audio path initializes; it cannot know whether a speaker is physically plugged in.
6. **Vendor markings:** the touch-controller suffix, fitted amplifier part number, and original factory partition table remain unresolved as detailed in [hardware.md](hardware.md). The chosen GPIOs are verified independently of these markings.

## Recorded mock board run

On 2026-09-29, `pio run -e codex-monitor` passed, and PlatformIO uploaded the landscape build over the board's USB serial link with hash verification. The board polled [the mock server](../monitor-server/MOCK.md) on the local LAN. The 115200-baud log recorded idle quotas 29%/51%, running and attention states, one attention playback attempt, completion to zero agents, HTTP 503 to `unavailable`, recovery to `connected`, and 0%/100% quota values with six active agents. After the redraw change, three unchanged polls generated no new status line; the owner said the landscape avatar view looked stable and readable. No flash erase-all or partition-layout change was made. The [hardware record](../test/hardware/codex-monitor-mock-2026-09-29.md) keeps the serial excerpts and limits.

## Next device check

Check touch selection, voice-button capture and mock WAV receipt, BLE volume/timeout writes, wake-on-touch, and audible speaker output with a connected speaker. Then configure the real bridge LAN endpoint and inspect live Codex thread visibility. The real voice transcription service remains intentionally unconfigured.


## WakeNet10 / VADNet / MultiNet integration (2026-10-03, build evidence)

Monitor 0.5.0 reuses the device-tested `speech-diag` models and convolution
kernel selection. “Hi ESP” opens a six-second command window for “start
listening”, “go back”, “show status”, “turn on the screen” and “turn off the
screen”. Local commands need no transcription service. The continuous AFE
feeds recognition and the voice recording buffer; VAD ends a recording after
its one-second silence debounce plus another second, with six seconds to start
speaking and a nine-second hard limit. Silence-only captures are discarded.
Recognition pauses during recording/submission and around attention tones.
Tap-to-stop remains available; speech never authorizes Codex approvals.

The monitor retains PlatformIO 7.0.1 and uses Arduino 3.3.12 as an IDF 5.5.5
component with its matching pinned GCC 14 compiler. Other Arduino apps retain
Arduino 2.0.17; speech/recorder retain IDF 6.0.1. Separate dependency locks avoid
mixing those component graphs. The monitor now uses a speech app/model layout with the Arduino NVS boundary,
which replaces the previous monitor's OTA/FATFS layout on upload. Both CLI and
browser image sets include the model partition. See the app README before
flashing; no upload or flash erase was performed for this integration.

Validation: `pio run -e codex-monitor` passed; its model image is 3,518,070 bytes
and contains `wn10_hiesp`, `vadnet1_medium`, `mn7_en` and the command FST. ELF
inspection confirmed the required unbiased WakeNet convolution kernel, USB
HWCDC serial and the legacy I²S implementation without the conflicting new
I²S entry point. `pio run -e speech-diag -e recorder -e audio-diag` also passed. All 18
native tests, the browser-package host test and 48 bridge regression tests
passed. An actual monitor-only browser package was generated
locally with bootloader, partition table, app and model images at the configured
offsets, without `boot_app0`.

Combined device behavior remains **UNKNOWN**. Before marking it device-ready,
check wake/command recognition, wake from screen-off, touch response, BLE
settings and saved Wi-Fi, SSE updates while recognizing speech, VAD recording
termination and bridge submission, attention-tone microphone recovery, heap
stability and inference time against each 32 ms frame. The firmware reports
VAD transitions, AFE frame counts, inference maxima and heap/PSRAM at 115200.

Secure monitor Wi-Fi provisioning and browser preservation (build verification):
- Monitor now reuses Security 1 + a fresh on-screen PoP in a separate setup boot.
- Hold the Wi-Fi status indicator for three seconds to request setup; no saved
  credentials opens setup automatically. Normal monitor BLE is not active then.
- Browser manifests request the erase choice instead of ESP Web Tools' implicit
  full erase; leave it unchecked. Monitor NVS matches the Arduino 20 KiB boundary.
- Physical update preservation, wrong-code rejection, provisioning/restart and
  cancellation checks: UNKNOWN until tested on the board.

On 2026-10-03 the user authorized a monitor upload. PlatformIO verified all image
hashes, and 115200-baud serial confirmed saved Wi-Fi reconnection, BLE advertising,
all three speech models ready, continuous AFE frames and the live Codex SSE stream.
Two hardware-only allocation failures were fixed by reserving the monitor feed
stack statically and allocating microphone DMA before speech models. The
[hardware record](../test/hardware/codex-monitor-speech-2026-10-03.md) includes
sanitized evidence and the remaining physical checks; internal heap is tight.

## Voice interaction and reset-bar update, 2026-10-04

Both header reset countdowns now use a five-pixel-high track with visible gray
for elapsed time and cyan/mint for remaining time. Unknown countdowns retain a
dash. “Hi ESP” opens a centered guide drawn from the same five phrases registered
with MultiNet, with an amber listening control and microphone-volume meter.
The command window is twelve seconds. The separate tap-to-talk message panel
allows ten seconds to start speaking, approximately five seconds of silence
before submission (existing one-second VAD debounce plus four seconds), and a
thirty-second maximum. A tap can stop early or take over from local command
listening. Recognition remains suspended during recording/submission, and
attention tones wait until command listening ends.

“Show status” holds the quota view even with active agents; “go back” or a tap
on the cards returns to the agent overview. All displayed agents can be selected
explicitly for a message, including running agents, using the bridge's existing
active-turn steering path. Avatars cannot overwrite command hints or message
panels, and touches on those panels cannot select hidden agents.

Validation: the final `pio run -e codex-monitor` passed (2,998,605 flash bytes,
122,188 static RAM bytes); all 20 native and 48 bridge tests passed. Host previews
checked command hints, silence/volume meter states, recording guidance, idle,
active, offline and quota endpoints at 320×240. The user subsequently authorized upload. PlatformIO verified all four image
hashes while retaining the existing partition layout and saved Wi-Fi. Serial
at 115200 confirmed the configured twelve-second MultiNet timeout, speech
models ready, continuous AFE frames, live SSE status and one running agent.
The actual LCD screenshot was refreshed through serial readback. See the
[device record](../test/hardware/codex-monitor-voice-ui-2026-10-04.md).
No full-chip erase or security-setting change was performed. Physical meter
sensitivity, spoken recognition timing/accuracy, pauses during actual recording,
30-second PSRAM capture and running-agent message interactions remain
**UNKNOWN** until exercised on the board.
