# Rear RGB LED diagnostic

Build with `pio run -e rgb-diag`. Upload only when intentionally replacing the
running monitor: `pio run -e rgb-diag -t upload`. Inspect serial with
`pio device monitor -b 115200`. The diagnostic exercises Solid, Blink, Breathe
and Rainbow at 20% brightness with a two-second cycle, first for working,
then completion and attention, followed by off. It uses the same portable
frame renderer as the monitor and writes at most every 40 ms. Phase/state/
pattern values are logged once per phase. Restore `codex-monitor` afterward.

The diagnostic preserves monitor partition boundaries and does not erase NVS.
The owner confirmed the earlier green/yellow/red/off sequence on 2026-10-14.
Animation appearance and BLE preference persistence require separate evidence.
