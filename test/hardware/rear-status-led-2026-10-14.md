# Rear status LED — 2026-10-14

PlatformIO Core CLI built `rgb-diag` and `codex-monitor` **0.8.2**. The diagnostic
was uploaded to the identified FNK0104B B81F3FC39F94 on `/dev/ttyACM0`; all four
image hashes verified. The owner confirmed green → yellow → red → off, two
seconds per phase. PlatformIO monitoring at 115200 through a Windows COM9
loopback relay captured repeated phases 0–3 without observed RGB/RMT errors.

The diagnostic was then replaced with `codex-monitor` 0.8.2; all five
bootloader/partition/OTA-initializer/model/application hashes verified, followed
by a successful watchdog reset. Both uploads preserved the existing monitor
partition boundaries and NVS. No erase-all or irreversible security changes
were performed. Normal USB ownership returned to Windows.

Post-upload PlatformIO serial monitoring at 115200 captured live Desktop
lighting revisions 4–8 with slot 0 working (`304ffe`) and the other slots idle
(`ffffff`). `health` confirmed version 0.8.2, Wi-Fi connected, both workers,
35,455 free internal heap bytes and 8,318,200 free PSRAM bytes. Microphone
remained muted, audio ready, USB audio drops zero. No panic appeared in this
short capture.

Between the working updates at 57,505 and 70,719 ms uptime, serial emitted
`monitor_led state=0 timeout_seconds=60 enabled=1`. Further working updates at
70,719 and 83,314 ms did not emit a new lighting event. This verifies the
one-minute shutoff and that identical host status does not restart it. The
initial green event preceded the captured serial window; the owner physically
verified colors in the isolated diagnostic, not each integrated transition.

Saved BLE LED enable/timeout and legacy-client preservation pass native and
browser-client tests. Physical BLE preference writes/persistence, integrated
attention/completion colors, and long-running USB/audio/Wi-Fi coexistence remain
UNKNOWN. USB completion depends on explicit known idle colors; raw blackout
updates remain ignored under the existing retention policy. Wi-Fi yellow means
active work ended and does not distinguish cancellation from success.

Validation: 39 native tests, 44 host tests, 20 browser-client tests, site tests,
site production build and responsive browser smoke checks passed. The updated
BLE setup page is served locally for acceptance; the public site/release has
not been published by this change.

Logs: [diagnostic upload](rear-status-led-2026-10-14/diag-upload.log),
[diagnostic serial](rear-status-led-2026-10-14/diag-runtime.log),
[monitor upload](rear-status-led-2026-10-14/monitor-upload.log),
[monitor serial](rear-status-led-2026-10-14/monitor-runtime.log).
