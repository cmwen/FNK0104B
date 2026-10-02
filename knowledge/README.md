# FNK0104B agent knowledge

Read [board.md](board.md), [pins.yaml](pins.yaml), and [references.md](references.md)
before hardware changes. This is an evidence index, not a replacement for
`docs/hardware.md`, `docs/pins.md`, or board support in `lib/fnk0104b`.

`pins.yaml` separates physical wiring, firmware use, and allocation restrictions.
An idle peripheral's GPIO is still wired to that peripheral. `UNKNOWN` means no
verified evidence; it never means available. Safe candidates are conditional on
checking the actual connector, attached hardware, and electrical requirements.
Source IDs resolve in `references.md`. Rechecked on 2026-10-02 against the official
Freenove schematic/specification and current repository code. Prior device
observations are attributed to `docs/hardware.md`, not repeated measurements.

## Tooling

Codex loads this repository's `.codex/config.toml` for trusted projects; no MCP
entries were added to user/global configuration. Espressif Documentation MCP
requires account authentication: run `codex mcp login espressif-docs` from this
repository and complete GitHub/WeChat sign-in. Start a new Codex session after
configuration changes; `/mcp` shows connected tools.

Use PlatformIO Core CLI directly. The third-party `platformio-mcp@3.1.0`
adapter was removed from this repository's `.codex/config.toml` at the owner's
request on 2026-10-02. Its build cache omitted `apps/`, and its independent
firmware-write approval flow added friction to the working CLI workflow.
The adapter offered device locks, logs and structured diagnostics, but these
did not justify keeping it for this project. Existing logs remain as historical
evidence. A new Codex session applies the updated MCP configuration.
Generic DevKitC pin advice does not describe FNK0104B.

The official ESP-IDF Tools MCP is omitted: it requires a standalone ESP-IDF 6.0+
MCP installation and targets IDF projects. This repository uses Arduino through
PlatformIO, with no `idf.py`/EIM installed. Do not migrate it to IDF for MCP access.
Build/flash/serial remain available through the existing PlatformIO Core CLI:
`pio run -e <environment>`, `pio run -e <environment> -t upload`, and
`pio device monitor -b 115200`. Upload verification must include serial output.

## Historical MCP verification (2026-10-02, before removal)

- Codex app-server `config/read` attributed both MCP entries to this repository's
  project layer; `codex mcp list` outside the repository listed neither entry.
- Codex app-server connected to PlatformIO MCP 3.1.0. `project_envs` resolved all
  21 environments; `build_project` with `environment: hello` and
  `forceExecution: true` succeeded through the existing PlatformIO installation.
  The direct `pio run -e hello` build also succeeded.
- `list_devices` returned no devices. Upload and serial-monitor tools were
  discovered by Codex but could not be exercised against hardware.
- Espressif Documentation MCP responded HTTP 401; Codex reported
  `authenticationRequired` / `notLoggedIn`. A documentation query remains
  unverified until the user completes sign-in.
- `pins.yaml` parsed successfully; GPIO coverage, source IDs, and display/touch/
  SD/RGB mappings were checked against current pin definitions.
- Existing firmware and build configuration files retained their pre-task hashes.
  No device upload, flash erase, partition-table change, or firmware edit occurred.

PlatformIO MCP created runtime logs/state under `.pio-mcp-workspace/`; these are
local execution artifacts, not hardware knowledge, and must not be committed.
