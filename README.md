# FNK0104B ESP32-S3 firmware lab

The [board field guide](https://cmwen.github.io/FNK0104B/) explains capabilities, firmware and lessons from the Codex monitor. Use [Flash & setup](https://cmwen.github.io/FNK0104B/setup.html#flash) to install published builds or configure the board. The Astro sources and local preview workflow are in [site/README.md](site/README.md).


PlatformIO Core CLI repository for small, independently buildable firmware apps targeting the Freenove FNK0104B (2.8-inch ILI9341 touch) board. Arduino is the starting framework. Board-specific GPIO facts and the initial board support API live in `lib/fnk0104b`; applications do not carry their own pin maps.

Read `docs/hardware.md`, `docs/pins.md`, and `docs/flashing.md` before hardware work. Verified facts and unresolved vendor-source conflicts are recorded there.
See `docs/development.md` for the pinned toolchain, CI setup, and current WSL debugging status. [Monitor readiness](docs/monitor-readiness.md) separates implemented targets from device-verified behavior and lists the open integration decisions.

## Commands

```bash
# Build the default hello firmware
pio run

# Build an app target
pio run -e hello
pio run -e display
pio run -e touch
pio run -e calculator
pio run -e hid-diag
pio run -e keyboard
pio run -e wifi
pio run -e wifi-ble
pio run -e codex-monitor
pio run -e audio-diag
pio run -e speaker-diag
pio run -e locallink
pio run -e ota
pio run -e sd
pio run -e file-manager
pio run -e button-diag
pio run -e speech-diag
pio run -e recorder-io-diag
pio run -e recorder

# Run host-side tests without a board
pio test -e native

# Upload a firmware
pio run -e hello -t upload

# Open a 115200 baud serial monitor
pio device monitor -b 115200

# Prepare a symbol-rich hello build for USB JTAG debugging
pio debug -e hello-debug
```

`hello`, `display`, and `touch` are independent diagnostics. The `calculator` environment combines the shared display and touch support. See `apps/09-calculator/README.md` for its controls.
`hid-diag` checks USB HID plus CDC before the `keyboard` app combines it with touchscreen input. The keyboard has a number pad, emoji shortcuts, and Windows/Mac/Linux selection. See [keyboard controls and flash effects](apps/23-keyboard/README.md).
`wifi-ble` pairs with the GitHub Pages flasher to save Wi-Fi credentials over a secure Bluetooth session. See `apps/15-wifi-ble/README.md`. `audio-diag` checks the onboard ES8311 microphone path by reporting signal presence without printing samples. `speaker-diag` plays notes through the PH1.25 speaker connector and provides a touchscreen volume slider. `locallink` combines mic capture, Wi-Fi/DNS-SD, HTTP transcription, and an LVGL touchscreen UI; configure private Wi-Fi values and see `apps/12-locallink/README.md` before flashing.
`ota` is a user-confirmed HTTPS update demo with touchscreen and serial controls. It downloads an `ota.bin` asset from the latest GitHub Release. See `apps/08-ota/README.md` for the release and install steps.
`sd` is the serial-only SDIO diagnostic. `file-manager` combines the shared SD, display, and touch support to browse folders, inspect card capacity, and read text or raw file bytes; all card access is read-only. See `apps/17-file-manager/README.md`.
`button-diag` reads a momentary button on GPIO14 and reports debounced press/release events over USB serial. See `apps/18-button-diag/README.md`.
`speech-diag` tests offline WakeNet10 (“Hi ESP”) followed by English MultiNet7 commands with an on-screen guide, listening countdown, last result, microphone meter and VADNet speech/silence indicator. USB serial also reports details. It uses an isolated ESP-IDF environment and a dedicated model partition; read [the speech diagnostic guide](apps/19-speech-diag/README.md) for the flash-layout effect before upload.
`recorder` records offline after “Hi ESP”, saves Ogg Opus clips to SD after silence, and provides touchscreen Record/Stop, file selection and speaker playback. `recorder-io-diag` checks its new IDF SD/touch/full-duplex audio combination. Read [the recorder guide](apps/20-recorder/README.md) for controls, validation limits and flash-layout effects.
`nvs` and `mqtt` remain buildable placeholders. `codex-monitor` has a status dashboard, voice capture, attention alerts, and BLE settings. USB Micro controls connect directly to Desktop without a local bridge. Firmware 0.6.3 defaults to Hold to talk, with an optional separate Voice Chat button and explicit outgoing microphone state; follow [the voice-control setup and implementation](docs/usb-micro-voice-controls.md). Wi-Fi dashboard and voice commands need a local bridge and, for transcription, a host speech service. Follow [the server-to-board setup guide](docs/monitor-setup.md), [firmware setup](apps/codex-monitor/README.md), and [the bridge reference](monitor-server/README.md). Recorded verification and open physical checks are in [monitor readiness](docs/monitor-readiness.md).

On WSL, USB/IP reattachment can change `/dev/ttyACM0` to `/dev/ttyACM1` or another number. Use the board's persistent `/dev/serial/by-id/` path from `docs/flashing.md` for repeatable monitoring.

## Independent publishing

Documentation and browser configuration changes publish through the guide workflow
without compiling firmware. Firmware source changes use a separate PlatformIO
workflow; successful main builds refresh the installer catalog. Host checks also
run independently. See [workflow triggers and manual runs](docs/ci-workflows.md).

## Repository map

- `apps/` — independent firmware apps selected by PlatformIO environment.
- `lib/fnk0104b/` — shared board support and verified pin map.
- `lib/network/`, `storage/`, `mqtt/`, `ota/`, `ui/` — reserved shared-library boundaries.
- `test/test_native/` — runnable host-side Unity test suite (`test/native/` explains PlatformIO's suite naming rule).
- `test/hardware/` — hardware checks and evidence status.
- `docs/` — board facts, pins, memory, flashing, monitor readiness, and sources.

## PlatformIO board target

PlatformIO does not list FNK0104B as a separate board. The environments use the supported generic `esp32-s3-devkitc-1` target with the FNK0104B's documented 16 MB flash and OPI PSRAM settings applied. That identifier names the PlatformIO software target only; it does not identify the physical board. The project selects PlatformIO's built-in `app3M_fat9M_16MB.csv`, matching the partition profile shown in Freenove's tutorial. This layout has two 3 MiB OTA app slots and a 9.9 MB FATFS partition. See `docs/memory.md` before upload or before changing the layout.
