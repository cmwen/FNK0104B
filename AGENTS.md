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

## Documentation and build journal

- Every new feature, behavior change, or project decision must include documentation updates in the same change. Treat documentation and the build journal as part of completing the work.
- Record the change in `site/src/data/journey.json`, newest first, with an Australia/Sydney timestamp. Write the public journey as the history behind the application and firmware: how the idea took shape, what needs or constraints prompted a change of direction, and how earlier choices led to later ones. Weave decisions and meaningful tradeoffs into connected prose. Group related changes into narrative chapters rather than logging every action or repeating “we chose.” Keep motivations grounded in the recorded history; do not invent anecdotes or intent. The page presents chapters from the beginning; the data stays newest first. Do not include development steps, debugging, build or flash operations, test results, validation checklists, release logs or remaining device checks. Omit milestones that only record engineering operations; link user-facing feature or decision documentation when useful.
- Use a verified commit SHA for committed milestones. For uncommitted work, use `sha: null` and the actual session timestamp; never invent a commit or attribute new work to an earlier commit. Update the journal entry with its verified SHA and commit timestamp once committed.
- Update the affected app README, setup/feature documentation, and guide/configuration page when applicable. Update `knowledge/`, `docs/hardware.md`, `docs/monitor-readiness.md`, and dated `test/hardware/` evidence when hardware facts or device validation change.
- Keep implementation details, automated checks, serial observations, owner confirmation and unverified physical behavior in the affected technical documentation and dated hardware evidence, not in the public journey. Preserve those evidence distinctions there. Do not describe local changes as published releases.
- Update this file when a decision changes how future agents should work; keep the application story in the journey and detailed feature history in linked technical documentation.

### Current monitor rear LED behavior

Codex monitor 0.8.3 includes a rear status indicator, enabled by default with a
60-second timeout. BLE Monitor settings can disable it or set 5–600 seconds,
saved on the board. Working, completion and attention each have independent
Solid/Blink/Breathe/Rainbow patterns, with shared brightness and cycle duration.
All patterns default to Solid and must respect the notification timeout.
Rainbow replaces the status color. Green means working, red means attention/error, and yellow
means observed work ended; it does not prove success. Repeated identical status
must not restart the timeout. Preserve the existing USB blackout-retention
policy. See `docs/monitor-status-led.md` and
`test/hardware/rear-status-led-2026-10-14.md` for semantics and evidence limits.

### Monitor settings persistence

Monitor 0.8.5 stores bridge host/port/token as one versioned `bridgeConfig`
record in the existing `monitor` NVS namespace. Encrypted connection setup uses
`monitor-bridge` under the board-code provisioning session; never expose or log
the token through ordinary BLE settings, readback, URLs or browser storage.
Preserve existing monitor/photo preference keys and older-client field behavior.
Compiled bridge defaults are only a one-time migration when the record is absent;
a saved override or explicit clear must always win. Keep normal OTA app-only.
See `docs/monitor-persistent-settings.md` for migration and evidence limits.

OTA metadata and image requests use the canonical `https://cmwen.dev/FNK0104B/`
origin. The old `cmwen.github.io` origin redirects and firmware up to 0.8.5
rejects it. Do not claim that a successful browser request proves device OTA;
verify redirect/status behavior and record physical recovery separately.

### Whistle standalone firmware

`whistle` uses an isolated IDF component/manifest under `apps/29-whistle`, selected
by `scripts/whistle_environment.py`, and its own dependency lock. Keep its engine
attribution/license and model source/image checksums intact. Prepare the model
explicitly with `scripts/prepare_whistle_model.py`; ordinary builds must not
fetch weights. Full uploads include the verified model and replace the monitor
layout with one app slot, so document the effect and restore the complete named
monitor environment when switching back. Keep board access in `lib/fnk0104b`.
Host inference/memory checks do not establish physical speech accuracy or timing.
