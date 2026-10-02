# Agent instructions

This repository targets the Freenove FNK0104B ESP32-S3 display board. PlatformIO Core CLI is the build and upload interface; do not require Arduino IDE.

## Hardware knowledge and MCP

- Read `knowledge/` before hardware changes; check `knowledge/pins.yaml` before assigning GPIOs.
- Use Espressif Documentation MCP (`espressif-docs`) for technical ESP32 questions; if unavailable, consult the authoritative links in `knowledge/references.md`.
- Use the existing PlatformIO Core CLI workflow and named environments for build/flash; MCP must not replace it with ESP-IDF.
- Use PlatformIO Core CLI directly for device discovery, builds, upload, and serial monitoring. The PlatformIO MCP adapter was removed from this repository's configuration.
- Do not invent board-specific facts. Mark unverified details `UNKNOWN` and update `knowledge/` when new hardware information is verified.

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

Do not add LVGL, Wi-Fi, MQTT, OTA, SD-card behavior, or Codex monitor features until asked. Existing apps implement several of these capabilities; `nvs` and `mqtt` remain buildable placeholders. Check `docs/monitor-readiness.md` for current monitor evidence and open checks.
