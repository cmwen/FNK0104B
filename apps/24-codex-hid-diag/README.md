# Codex vendor HID diagnostic

`pio run -e codex-hid-diag` builds a small protocol-only firmware on the existing
Arduino 2.0.17 stack. It has vendor HID report 6 and CDC serial. Send `t` over
115200-baud serial to queue AG00 press/release. It never generates input at boot.
See [the compatibility report](../../docs/codex-hid.md) for host checks.

This diagnostic uses the Arduino partition profile and replaces the monitor's
model/app layout if uploaded. Prefer uploading the combined `codex-monitor`
environment when preserving the running monitor. Building the diagnostic alone
does not change the device.
