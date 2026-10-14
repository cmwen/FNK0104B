# Rear RGB LED diagnostic

Build with `pio run -e rgb-diag`. Upload only when intentionally replacing the
running monitor: `pio run -e rgb-diag -t upload`. Inspect serial with
`pio device monitor -b 115200`. Green, yellow, red and off alternate every two
seconds at low brightness. Restore `codex-monitor` after checking the colors.
This diagnostic retains the monitor partition boundaries and does not erase
NVS. The owner confirmed the expected color sequence on 2026-10-14.
Integrated monitor settings and timing still require device acceptance.
