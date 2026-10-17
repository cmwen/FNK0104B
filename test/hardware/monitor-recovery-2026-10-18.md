# Monitor USB recovery — 18 October 2026

The owner attached a board running 0.8.3 after the configuration page could not
fetch the update catalog. PlatformIO serial capture reproduced **Update catalog
unavailable** before recovery. See the [catalog address investigation](monitor-ota-catalog-2026-10-18.md)
for the independently verified redirect and published artifacts.

## Device and recovery

The application enumerated as `303a:8360`, serial `B81F3FC39F94`, Windows COM9.
Windows held the Micro interface; the owner approved the administrator prompt
for a temporary usbipd transfer to WSL. ROM upload mode enumerated as
`303a:1001`, serial `B8:1F:3F:C3:9F:94`, on `/dev/ttyACM0`.

The named PlatformIO `codex-monitor` environment built 0.8.6 before upload.
The first 1200-baud reset lost the application port before any image write.
Rediscovery found the same board in ROM mode; repeating the named upload
succeeded with all five image hashes verified. This was a full USB recovery
using the existing layout: NVS at `0x9000–0xdfff` was untouched. No erase-all,
eFuse or security-setting operation was used. The private local build retained
the existing ignored bridge defaults so the new migration could save them.
No credential values were printed or included in these records.

## Runtime observations

At 115200 baud, 0.8.6 reported Wi-Fi connected, the bridge integration connected,
three slots, avatars enabled and LED notifications enabled with a 300-second
timeout. These sampled settings matched the captured 0.8.3 baseline. This
establishes runtime continuity after USB recovery, not a readback of every
preference or the secret NVS record. Secure BLE save/clear and retention across
an actual OTA remain unverified.

The recovered board still failed two catalog requests at HTTP header fetching.
With speech initialized, free internal heap was approximately 5.5–6 KiB.
The 0.8.6 canonical address correction therefore did not establish working OTA.
A 0.8.7 diagnostic build reduces HTTP RX/TX buffers from 4096/1024 to 1024/512
bytes and reports transport error, status and heap information on header failure.
The build and upload succeeded with all image hashes verified. After restarting,
the board returned to its normal Windows driver (`IsForced: false`, no USBIP
client). The auto-attach loop was stopped. PlatformIO monitored Windows COM9
through a loopback-only serial relay at 115200 baud.

With Desktop USB active, 0.8.7 reported approximately 34 KiB of internal heap
and speech not initialized. Two live catalog requests completed with
**Installed firmware is current or newer** (published catalog: 0.8.6). The bridge
reconnected after each check. The same sampled preferences remained present.
This verifies catalog access in USB Micro mode. It does not isolate the buffer
change from the larger available heap, and does not establish that the earlier
speech-loaded failure is fixed. That path remains **UNKNOWN**; memory pressure
is a hypothesis, not a proven cause.

Raw serial captures are in [the evidence directory](monitor-recovery-2026-10-18/).
Actual wireless application install, interrupted download and boot rollback
remain **UNKNOWN**.

The final 0.8.7 PlatformIO monitor build passed. All four guide checks passed.
No public journey chapter was added for these engineering-only recovery and
diagnostic observations.
