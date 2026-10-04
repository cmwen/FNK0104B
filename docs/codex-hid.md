# Concurrent Wi-Fi and unofficial Codex Micro HID

Engineering report, 2026-10-04. Compatibility is a private, unofficial protocol;
USB enumeration is verified. The owner subsequently confirmed that Desktop recognizes it as Codex Micro; initial runtime gaps below remain as historical evidence.

## Existing architecture and change

`codex-monitor` uses Arduino 3.3.12 as an ESP-IDF 5.5.5 component, through the
existing pinned PlatformIO 7.0.1 CLI environment. Wi-Fi/SSE already has a worker,
with a separate voice worker and a main touch/display loop. The Wi-Fi snapshot
contains identities and semantic states from the existing orchestrator.

The new worker runs independently before Wi-Fi provisioning starts. USB loss
only clears HID reassembly, pending traffic and Desktop status. Wi-Fi failures,
quiet mode and provisioning never stop the HID worker. There is no transport
mode switch. Existing Wi-Fi, BLE provisioning, speech, UI and partition boundaries
are retained; pre-existing dispatcher changes in the working tree were preserved.

USB moves from Serial/JTAG to OTG, with CDC plus one vendor HID interface.
Board USB access stays in `lib/fnk0104b`. Native CDC still carries Arduino
`Serial` at 115200 baud; early IDF/boot logs use the existing UART0 connector,
and built-in USB-JTAG is unavailable during OTG operation. No GPIO assignments,
eFuses, security settings or erase-all operation changed. Upload rewrites the
same bootloader/app/model/table image locations; it does not erase NVS.

## Files changed for HID

- `lib/codex_hid/src/codex_hid/{wire,protocol,backend}.hpp` and `src/backend.cpp`:
  bounded framing, discovery/status RPCs, status mailbox and independent worker.
- `lib/fnk0104b/src/codex_usb.cpp` and `src/fnk0104b/codex_usb{,_identity}.hpp`:
  board USB transport, report descriptor registration and compiled USB identity.
- `components/arduino_tinyusb/`: official Arduino USB component configuration,
  adapted to fetch an immutable TinyUSB commit without changing installed packages.
- `CMakeLists.txt`, `apps/CMakeLists.txt`, `apps/codex-monitor/sdkconfig.defaults`
  and `platformio.ini`: USB component, OTG/CDC/HID selection, native JSON tests,
  and the isolated `codex-hid-diag` environment. Toolchain pins are unchanged.
- `apps/codex-monitor/src/main.cpp` and its README: startup, status adapter and
  explicit serial `hid-agent0` command. Wi-Fi snapshot/control code is unchanged.
- `apps/24-codex-hid-diag/`: protocol-only diagnostic; build without uploading
  when retaining the running monitor's partition profile.
- `test/test_native/test_main.cpp`, `scripts/probe_codex_hid.py`, this report,
  `test/hardware/codex-hid-2026-10-04.md` and `knowledge/board.md`: verification
  and evidence.

## Research and assumptions

Source snapshots inspected directly on 2026-10-04:

| Reference | Commit | Useful evidence |
| --- | --- | --- |
| [codex-micro-open](https://github.com/arthurcolle/codex-micro-open) | `3ea3db39c85ca3240e9bee8d1bb0551bf34b1e63` | Physical report-map/status captures; compact RPC and CRLF framing |
| [fttawa/codex-micro](https://github.com/fttawa/codex-micro) | `80d20ba1eec66071261505e089e0851cd91761eb` | Newline-free host calls, compact input events and discovery replies |
| [codex-island-esp32s3](https://github.com/lxw666598/codex-island-esp32s3) | `0116fb506da813102651d78467cddb607fbdcb82` | ESP32-S3 vendor-first collection and long-form RPC interoperability |

The independent implementation extracts the wire contract rather than copying
another device's application, board mapping or UI. Reference-device observations
are not proof of this board's compatibility with the installed Desktop version.

| Item | Implemented contract |
| --- | --- |
| Identity | VID `303a`, PID `8360`; product `Codex Micro`, manufacturer `Work Louder`; framework MAC-derived unit serial, not a copied serial |
| HID | Vendor page `ff00`, top-level usage 1, report ID 6; 63-byte input/output bodies plus ID = 64-byte wire reports; input usage 2/output usage 3 |
| Framing | Opcode/channel 2, chunk length 0–61, payload, zero padding; accept balanced JSON objects without newline or with LF/CRLF; transmit CRLF |
| JSON | Accept `m/p` and `method/params`; preserve string/integer request IDs; replies use `result`/`error`, `id`, `method` |
| Discovery | `device.status`, `sys.version`; honest firmware version `0.1.0-fnk0104b-hid`, profile 0/layer 1 |
| Power | Battery 100 and charging false are compatibility placeholders for USB operation; no battery/charger measurement is claimed |
| Host status | `v.oai.thstatus` updates six bounded slot records: raw color, brightness, effect, speed; acknowledge with `result.ok=1`, including id-less notifications |
| Other calls | Runtime ACK-only handling of `v.oai.rgbcfg`, `lights.preview`, `host.focused_app`; unknown requests get `-32601`, unknown notifications are ignored |
| Device input | Explicit AG00 press/release: `m=v.oai.hid`, `p={k:"AG00",act:1/0,ag:0}`; no unsolicited input |

The retail device additionally has keyboard, consumer, mouse and gamepad reports.
This iteration exposes only report 6, plus CDC. Framework-generated composite
configuration/interface numbering differs from retail. Vendor-first HID usage is
preserved. Whether Desktop also checks omitted collections/version/configuration
is UNKNOWN. There is no evidence that this iteration needs USB audio or BLE HID.

Espressif's [USB device documentation](https://docs.espressif.com/projects/esp-idf/en/v6.0/esp32s3/api-reference/peripherals/usb_device.html)
and [Arduino component instructions](https://docs.espressif.com/projects/arduino-esp32/en/latest/esp-idf_component.html)
were consulted because authenticated Espressif MCP tools were unavailable.
CDC consumes two IN endpoints and one OUT; HID consumes one IN and one OUT.
Keyboard can later share this HID interface with another report ID. Future audio
needs an explicit endpoint/bandwidth plan within S3 limits.

## Reliability and application adapter

Callbacks validate lengths and queue fixed-size reports without blocking. The
worker has a 2 ms yield, fixed JSON documents, a 2048-byte receive ceiling,
2-second partial-message expiry, bounded nesting and four queued responses.
Quoted braces and escapes are handled correctly; multiple objects per report
remain separate responses. Incoming queue overflow changes the transport epoch
so partial streams cannot silently continue. Reconnect discards old epochs.
Transmit advances only after TinyUSB accepts a packet, with a 500 ms bounded
retry period. HID paths allocate queues once, not per message.

The main loop retains a separate Desktop status mailbox even during Wi-Fi setup.
Raw lighting slots do not contain orchestrator IDs, titles or dependable semantic
states. They do not overwrite Wi-Fi agents and are not yet displayed. Logs expose
USB mount state, report activity, RPC methods, response/event transmission and
status adapter receipt, without payload dumps. `host=active` means HID traffic,
not independently confirmed Codex Desktop discovery. Input is queued as one tap
and remains separate from Wi-Fi voice/action dispatch.

## Verified locally

- `pio run -e codex-monitor`: passed, approximately 3.08 MB of the unchanged
  6 MB app partition; static RAM approximately 144 KB. Existing upstream
  deprecation/initializer warnings remain.
- `pio run -e codex-hid-diag`: passed. Its final transport rebuild is recorded in
  the hardware evidence file.
- `pio test -e native`: **24 passed**, including descriptor page/ID/report bit
  counts, fragmented JSON, multiple calls/report, escapes, malformed lengths,
  overflow recovery, compact/long-form RPC, status validation and event encoding.
- Host tests: **5 passed**; monitor-server tests: **60 passed**.
- `git diff --check` and host probe syntax compilation passed.
- Combined firmware uploaded to the attached board through PlatformIO: all
  bootloader/table/model/app hashes verified; successful post-upload reset.
- Windows `usbipd list` then reported bus `3-1`, **303a:8360**, USB Input Device
  and USB Serial Device **COM9**. This proves physical composite enumeration.
- WSL lost the previous attachment across the identity change. PlatformIO's
  required 115200-baud serial check was attempted but the old `/dev/ttyACM0`
  no longer existed. Boot completion, display/touch, live Wi-Fi/SSE, host RPC,
  Desktop detection, reconnect and input acceptance still require runtime checks.

## Test with Codex Desktop

Keep the USB device owned by the computer running Desktop during its test.
USB/IP attaches the whole device to WSL; Desktop on Windows cannot then own HID.

1. The combined firmware is already flashed. For later builds/uploads:
   `pio run -e codex-monitor`, `pio device list`,
   `pio run -e codex-monitor -t upload --upload-port <port>`.
   To apply the new defaults to an old cached build, first back up and remove
   the ignored `sdkconfig.codex-monitor`; do not edit the partition table.
   If USB CDC cannot reset into upload mode, use the board's BOOT/RESET procedure.
2. Check Windows Device Manager for **303a:8360**, HID and COM9 (the COM number
   may change). `usbipd list` provides a compact check. On Linux use
   `lsusb -d 303a:8360`, `lsusb -v -d 303a:8360`, and inspect the corresponding
   `/sys/bus/hid/devices/*/report_descriptor` for vendor page ff00/report 6.
   Linux HID access may require a local hidraw permission rule.
3. For PlatformIO monitoring from WSL, first bind bus `3-1` in an administrator
   Windows terminal: `usbipd bind --busid 3-1`; then
   `usbipd attach --wsl --busid 3-1`. Rediscover the CDC port and run
   `pio device monitor -p <port> -b 115200`. Expect monitor startup, speech
   heartbeats, `monitor_wifi connected=yes`, and continuing `monitor_status`
   alongside `codex_hid usb=mounted`. Check the physical screen and touch.
4. Close Desktop for an independent probe. In a local Python venv install
   `hidapi`, then run `python scripts/probe_codex_hid.py --list` and
   `python scripts/probe_codex_hid.py --send-status --listen 20`.
   Expect status/version replies, `result.ok=1`, and device logs
   `status_received` followed by `application=status_retained`. While listening,
   enter `hid-agent0` plus Enter in monitor serial; expect **both** AG00 events
   with actions 1 and 0. The diagnostic firmware uses `t` instead.
5. If returning ownership to Windows, run `usbipd detach --busid 3-1` first.
   Close the probe and open Codex Desktop on that host. Check its device/settings
   UI and logs for discovery and status/version calls; the firmware should log
   decoded methods and completed replies. Start or change a Desktop session and
   look for actual `v.oai.thstatus` traffic. A synthetic probe status does not
   satisfy the Desktop status criterion. Send AG00 once and check Desktop's
   expected agent-selection behavior; firmware `tx=complete` alone proves only
   USB queue acceptance.
6. Disconnect/reconnect USB while independently powering the board, if the
   existing battery setup is available, to check Wi-Fi persistence without a
   power cycle. Without independent power, unplugging USB reboots the whole
   board and cannot prove transport isolation. Also interrupt Wi-Fi at the
   router while keeping USB powered: status/version RPC must remain responsive.

## Compatibility checklist

Checked boxes mean observed on this board during this task.

- [x] ESP32 boots normally through application startup
- [ ] Existing display/touch behavior works
- [x] Existing Wi-Fi integration works
- [x] USB vendor HID + CDC enumerates with 303a:8360 on Windows
- [ ] Host report descriptor readback matches expected vendor contract
- [x] Codex Desktop detects device (owner observation)
- [x] Windows-side host discovery RPC received (application identity not authenticated)
- [ ] ESP32 -> Desktop event received
- [ ] Codex agent status received from Desktop
- [ ] USB reconnect works
- [x] Wi-Fi remains active during HID use
- [ ] HID remains responsive during Wi-Fi outage

## Current limitations and next step

The owner confirmed discovery by Desktop. The final audio follow-up also verified
normal startup, local speech, Wi-Fi/SSE and a live Windows-side device.status
request/reply. Physical input acceptance, Desktop status semantics, reconnect and
Wi-Fi-outage independence remain unverified. The waveform shows local microphone
input, not a remote transcript. ACK-only methods do not drive RGB hardware.

The initial HID-only iteration's audio exclusion is superseded by the microphone
follow-up below. Compatibility identities are for an unofficial experiment;
no official support or retail parity is claimed.

## Follow-up: microphone and Desktop controls

The owner reported Desktop discovery, then requested a Micro status indicator,
USB microphone, ACT10 voice control and correction of the persistent local
“Mic unavailable” state. See [the follow-up implementation and checks](codex-audio.md).
Its audio capability supersedes the initial iteration's audio exclusion; the
historical verification section above applies to the earlier flashed HID firmware.
