# FNK0104B ESP32-S3 firmware lab

PlatformIO Core CLI repository for small, independently buildable firmware apps targeting the Freenove FNK0104B (2.8-inch ILI9341 touch) board. Arduino is the starting framework. Board-specific GPIO facts and the initial board support API live in `lib/fnk0104b`; applications do not carry their own pin maps.

Read `docs/hardware.md`, `docs/pins.md`, and `docs/flashing.md` before hardware work. Verified facts and unresolved vendor-source conflicts are recorded there.
See `docs/development.md` for the pinned toolchain, CI setup, and current WSL debugging status.

## Commands

```bash
# Build the default hello firmware
pio run

# Build an app target
pio run -e hello
pio run -e display
pio run -e touch
pio run -e calculator
pio run -e wifi
pio run -e wifi-ble
pio run -e codex-monitor
pio run -e audio-diag
pio run -e locallink

# Run host-side tests without a board
pio test -e native

# Upload a firmware
pio run -e hello -t upload

# Open a 115200 baud serial monitor
pio device monitor -b 115200

# Prepare a symbol-rich hello build for USB JTAG debugging
pio debug -e hello-debug
```

`hello`, `display`, and `touch` are independent diagnostics. The `calculator` environment combines the verified display and touch support. See `apps/09-calculator/README.md` for its controls.
`wifi-ble` pairs with the GitHub Pages flasher to save Wi-Fi credentials over a secure Bluetooth session. See `apps/15-wifi-ble/README.md`. `audio-diag` checks the onboard ES8311 microphone path by reporting signal presence without printing samples. `locallink` combines mic capture, Wi-Fi/DNS-SD, HTTP transcription, and an LVGL touchscreen UI; configure private Wi-Fi values and see `apps/12-locallink/README.md` before flashing.

On WSL, USB/IP reattachment can change `/dev/ttyACM0` to `/dev/ttyACM1` or another number. Use the board's persistent `/dev/serial/by-id/` path from `docs/flashing.md` for repeatable monitoring.

## Repository map

- `apps/` — independent firmware apps selected by PlatformIO environment.
- `lib/fnk0104b/` — shared board support and verified pin map.
- `lib/network/`, `storage/`, `mqtt/`, `ota/`, `ui/` — reserved shared-library boundaries.
- `test/test_native/` — runnable host-side Unity test suite (`test/native/` explains PlatformIO's suite naming rule).
- `test/hardware/` — proposed hardware integration test plan.
- `docs/` — board facts, pins, memory, flashing, and sources.

## PlatformIO board target

PlatformIO does not list FNK0104B as a separate board. The environments use the supported generic `esp32-s3-devkitc-1` target with the FNK0104B's documented 16 MB flash and OPI PSRAM settings applied. That identifier names the PlatformIO software target only; it does not identify the physical board. The project selects PlatformIO's built-in `app3M_fat9M_16MB.csv`, matching the partition profile shown in Freenove's tutorial. This layout has two 3 MiB OTA app slots and a 9.9 MB FATFS partition. See `docs/memory.md` before upload or before changing the layout.
