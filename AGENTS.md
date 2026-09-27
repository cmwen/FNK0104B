# Agent instructions

This repository targets the Freenove FNK0104B ESP32-S3 display board. PlatformIO Core CLI is the build and upload interface; do not require Arduino IDE.

## Before changing hardware code

- Read `docs/hardware.md` and `docs/pins.md` first.
- Never guess GPIO mappings, controller variants, memory sizes, or peripheral setup. If a detail is not verified by the cited Freenove resources, record it as unknown and ask before using it.
- Keep board-specific pin assignments and hardware access in `lib/fnk0104b`. Do not copy shared board code into apps.
- Give each hardware capability a small diagnostic firmware before combining capabilities.
- Prefer host-side tests for hardware-independent logic.
- Keep apps independently buildable with their named PlatformIO environments.
- Keep the pinned platform version in `platformio.ini` and the CI workflow aligned; build the affected apps and run native tests when updating the toolchain.
- Do not hard-code Wi-Fi credentials, broker passwords, tokens, or other secrets into source control.

## Build and device work

- Build the selected environment before attempting upload.
- After upload, inspect serial output at 115200 baud.
- Do not erase flash or alter a partition table without documenting the effect.
- Do not modify eFuses, Secure Boot, Flash Encryption, bootloader security settings, or other irreversible security settings without explicit user approval.
- Never use `espefuse.py` or an equivalent command unless explicitly approved.

## Scope

Do not add LVGL, Wi-Fi, MQTT, OTA, SD-card behavior, or the Codex monitor dashboard until asked. The current stubs only establish independent build targets.
