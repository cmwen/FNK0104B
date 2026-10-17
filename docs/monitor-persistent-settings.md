# Persistent monitor configuration (0.8.5)

Bridge address, port and access token now live in the board's existing NVS
storage alongside monitor preferences. Compatible public OTA updates replace
only the application, so neither those values nor saved Wi-Fi need to be
re-entered. No partition, model, bootloader or security-setting changes are made.

## Configure the bridge with public firmware

1. Install monitor 0.8.5. Keep the board powered and on your saved Wi-Fi.
2. Hold the screen's Wi-Fi indicator for three seconds. The board restarts into
   connection setup and displays a new 12-character code.
3. Open [Device setup](https://cmwen.github.io/FNK0104B/setup.html#setup) in Chrome
   or Edge, enter that code in the Wi-Fi/connection setup section and connect to
   `FNK0104B-SETUP`. Wait for **Connected securely**.
4. In the **Monitor bridge** card beside Wi-Fi (below it on smaller screens), enter the computer's LAN IPv4 address or hostname,
   port (usually 8765), and the same token used by the bridge on the computer.
   Enter only the host, without `http://`, a port suffix or a URL path.
5. Choose **Save bridge and restart**. The board saves the configuration and
   restarts after about three seconds, keeping the existing Wi-Fi network.

A blank token field preserves an existing saved token; the token is never read
back. **Remove saved bridge and restart** explicitly disables the bridge and
removes its saved token. It does not reset Wi-Fi or monitor preferences. The
browser clears its token input after a save attempt or disconnection, and does
not put it in browser storage, URLs or status messages.

Wi-Fi and bridge use separate submissions. When changing both, save Wi-Fi first;
that flow restarts the board. Open connection setup again to save the bridge.
The bridge form stays disabled on older firmware. Ordinary Monitor settings
remain a separate BLE connection for preferences and OTA controls.

## Migration and retained preferences

A locally built 0.8.5 firmware with the old ignored `monitor_secrets.h` copies
valid compiled bridge values into NVS on first boot when no bridge record exists.
Thereafter saved values take priority, including an explicit removal. A public
build cannot recover bridge credentials that existed only inside a previous
firmware image. After upgrading from 0.8.4 or earlier through public OTA, enter
those values once using the form above. No future private build is needed.

These existing preference keys are preserved without renaming or resetting:

| Settings | NVS keys in the `monitor` namespace |
|---|---|
| Volume, screen timeout | `volume`, `timeout` |
| Agent layout, avatars, microphone controls | `microSlots`, `microAvatars`, `usbVoice` |
| Rear LED enabled and timeout | `statusLed`, `ledSeconds` |
| Rear LED patterns, brightness and speed | `ledWorking`, `ledComplete`, `ledAttention`, `ledBrightness`, `ledPeriod` |
| Idle/photo mode and timing | `idleMode`, `photoDelay`, `photoInterval` |
| Bridge configuration | `bridgeConfig` (one versioned JSON record) |

Missing preference keys get their established defaults. Older BLE packet
versions only write fields they support. Preferences already persisted before
0.8.5; their load/save paths are now shared with regression tests. USB audio still
starts muted and live voice sessions are not resumed after reboot. Robot seeds,
selected tiles and transient screen state are not saved preferences. Photo files
stay on SD. Desktop action mappings remain on the computer.

An invalid or unsupported bridge record fails closed without silently restoring
compiled values. Saving a new valid configuration repairs it. Erase-all, an
explicit factory reset or an incompatible partition migration can remove saved
settings; normal compatible OTA does not.

## Protocol and validation

The `monitor-bridge` custom endpoint is created before network provisioning
starts and registered afterward, using the existing Security 1 session and
board-displayed proof of possession. It is not exposed as an unprotected
Monitor settings characteristic. The pinned `esp-ble-prov` client encrypts its
custom-endpoint writes and decrypts responses through that same session.

Requests are JSON, at most 480 bytes:

- `{"op":"read"}` returns host, port and `has_token`, never the token.
- `{"op":"save","host":"pc.local","port":8765,"token":"example-only"}`
  persists one record. Omit `token` to keep the stored key.
- `{"op":"clear"}` persists an empty record so compiled defaults cannot return.

Responses include `ok`, `host`, `port` and `has_token`, with a short error code on
failure. Hosts are 1–127 ASCII hostname/IPv4 characters, ports 1–65535, and tokens
0–128 printable ASCII characters excluding spaces/control characters. Embedded
NULs, header injection and oversized records are rejected. Configuration changes
apply after reboot, so status and voice workers always share one configuration.
No token is logged. NVS uses the existing flash-storage configuration; this does
not add encryption at rest or change eFuses.

Native tests cover migration to a public build, saved overrides, explicit clear,
corrupt/newer records, storage failures, request validation, token preservation,
all preference fields and old-client writes. Browser tests cover encrypted-session
gating, older firmware, secret-free readback, save/clear, invalid inputs and
secret clearing on failure/disconnect. Physical BLE save/reboot, retention across
real OTA and live bridge reconnection remain **UNKNOWN** until tested on a board.

Sources: [Espressif protocomm](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/provisioning/protocomm.html),
pinned `managed_components/espressif__network_provisioning/include/network_provisioning/manager.h`
and `web-flasher/node_modules/esp-ble-prov/dist/index.esm.js`.
