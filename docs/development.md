# Development toolchain

This repository uses PlatformIO Core CLI. No Arduino IDE is required.

## Pinned stack

| Layer | Version / policy | Reason |
|---|---|---|
| PlatformIO Core | 6.2.0 locally and in CI | Pinned and checked locally on 2026-09-29; install in a user environment, without `sudo`. |
| Espressif32 platform | `platformio/espressif32@7.0.1` in `platformio.ini` | Pinned platform; keep the local and CI builds on the same version. |
| Arduino-ESP32 framework | 2.0.17, selected by the official PlatformIO platform | The platform still packages this version. Moving to Arduino 3 needs a separate compatibility migration. |
| Native platform | `platformio/native@1.2.1` | Pinned for host tests. |
| Host Python | 3.12 in CI | Explicit setting in `.github/workflows/`; PlatformIO Core manages build packages. |
| Host Node.js | 24 in CI | Builds the optional browser flasher. |
| WSL Python / Git / Codex CLI | Host-dependent | None is a firmware target version. Check installed versions locally when diagnosing a host issue. |

Install PlatformIO Core using the [official CLI installation guide](https://docs.platformio.org/en/latest/core/installation/methods/installer-script.html) or `python3 -m pip install --user 'platformio==6.2.0'` in an environment that permits user installs. Confirm with `pio --version`. The WSL installation already has Core 6.2.0; do not install a second copy merely to run this repository.

Build with `pio run` or `pio run -e <app>`. Run `pio test -e native` for host tests. The GitHub Actions workflow builds changed or uncached firmware targets, runs native tests, and packages a complete catalog of new and reused firmware images for the GitHub Pages browser flasher; it never uploads to a board. The platform pin fixes the version used by local builds and CI. Review [PlatformIO's Espressif32 releases](https://github.com/platformio/platform-espressif32/releases) and rebuild before changing it.

The official PlatformIO 7.0.1 release includes ESP-IDF 6.0.1 as an *alternative framework*. These applications still use Arduino 2.0.17, which is based on ESP-IDF 4.4.7. Shared low-level code can be migrated deliberately when an ESP-IDF application is introduced. This is a pin, not a claim that newer releases should automatically replace it.

## USB and debugging on this WSL host

USB serial upload and 115200-baud monitoring worked on 2026-09-27 when `usbipd` attached the board and `dialout` group membership was active. USB detached during some resets; Windows `usbipd attach --wsl --busid 3-1 --auto-attach` reattached it. The PowerShell auto-attach loop must remain running for that behavior. A later upload with platform 7.0.1 verified successfully. The user subsequently captured `status=running`, followed by an `Input/output error` when USB/IP disconnected. The board reappeared as `/dev/ttyACM1`; see `docs/flashing.md` for its persistent `/dev/serial/by-id/` path. Monitoring that link then received four hello heartbeats over roughly 30 seconds with no disconnect. A complete startup banner is still pending.

Source-level JTAG debugging is **not yet verified**. The earlier `pio debug -e hello --interface=gdb` attempt failed because PlatformIO's bundled Xtensa GDB requires `libpython2.7.so.1.0`, absent on this Ubuntu installation. The `hello-debug` environment builds the same hello app with symbols and selects the standalone Espressif GDB declared by the current PlatformIO platform. That GDB starts on this host without Python 2.7. Use `pio debug -e hello-debug --interface=gdb` after USB permissions are configured. Its `debug_load_mode = manual` avoids an automatic firmware write during debugger launch.

A direct OpenOCD attempt could not open the USB JTAG interface (`LIBUSB_ERROR_ACCESS`); serial `dialout` permission does not grant JTAG USB access. [Espressif's ESP32-S3 JTAG guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-guides/jtag-debugging/configure-builtin-jtag.html) requires Linux udev rules for OpenOCD. The repository provides `scripts/99-fnk0104b-jtag.rules`, scoped to the board's observed USB ID `303a:1001` and Ubuntu's `plugdev` group. `cmwen` is already in that group. Install it once in WSL with administrator access:

```bash
sudo install -m 0644 scripts/99-fnk0104b-jtag.rules /etc/udev/rules.d/99-fnk0104b-jtag.rules
sudo udevadm control --reload-rules
sudo udevadm trigger --subsystem-match=usb
```

Then detach and reattach the board through `usbipd` (or unplug/replug it), verify the matching `/dev/bus/usb/` node is accessible to `plugdev`, and retry `pio debug -e hello-debug --interface=gdb`. Do not claim breakpoint debugging works until OpenOCD connects and GDB reaches the target. A newer Espressif32 platform pin alone does not change its Arduino 2.0.17 compiler toolchain or bundled OpenOCD version. See `docs/flashing.md` for the working serial path.

## Sources

- [PlatformIO Core releases on PyPI](https://pypi.org/project/platformio/)
- [PlatformIO Espressif32 7.0.1 release](https://github.com/platformio/platform-espressif32/releases/tag/v7.0.1)
- [PlatformIO Native releases](https://github.com/platformio/platform-native/releases)
- [Arduino-ESP32 releases](https://github.com/espressif/arduino-esp32/releases)
- [PlatformIO GitHub Actions guide](https://docs.platformio.org/en/latest/integration/ci/github-actions.html)

## Isolated speech framework

`speech-diag` uses the pinned `platformio/espressif32@7.0.1` platform's
ESP-IDF 6.0.1 framework with ESP-SR 2.5.5. It is built through PlatformIO Core CLI in
CI, alongside existing Arduino applications. Root CMake files are used only by
this IDF target. Its sdkconfig defaults are in `apps/19-speech-diag`; generated
`sdkconfig.speech-diag` and `managed_components/` are local build products.
`dependencies.lock` records the exact resolved speech dependencies for CI.
The model-data image is packed and added to CLI uploads by a post script.
The browser flasher remains Arduino-only because this firmware has a different
partition layout and image set. See [speech diagnostic](../apps/19-speech-diag/README.md)
before upload. `hello-debug` now explicitly selects `esp-builtin`, matching the
speech target. Breakpoint debugging remains unverified on physical hardware.


## Speech-enabled monitor runtime

`codex-monitor` uses Arduino-ESP32 3.3.12 as an ESP-IDF 5.5.5 component via the
same pinned PlatformIO platform. This preserves its Arduino UI/network/BLE
interfaces while linking the existing ESP-SR 2.5.5 PoC. GCC 14.2.0+20260121 and
`dependencies.monitor.lock` pin its separate runtime. The other IDF apps use
`dependencies.lock` and the platform's IDF 6.0.1 / GCC 15 toolchain.
`scripts/monitor_idf_compat.py` redirects the platform's two linker-preprocessor
actions to the repository helper and supplies IDF text-asset embedding actions;
it does not modify downloaded packages. The CI build script still builds the
named environments, and CI also checks the browser monitor image package.
The monitor's app README documents the partition change before any upload.

## Incremental firmware CI — 2026-10-08

Actions previously ran `scripts/build_firmware.py` on every push, including
website and documentation changes. It now plans a cache key per named firmware
environment using `scripts/firmware_ci.py`. The 28-environment matrix restores
exact portable output bundles and verifies input identity and image SHA256 values.
An exact valid hit skips PlatformIO installation and firmware compilation. A
missing, expired or invalid entry builds that environment with `pio run -e`.
The first run fills the caches and therefore builds everything once.

Inputs include the app source tree, inherited PlatformIO source selection and
build scripts, libraries reached through transitive includes, and explicitly
compiled IDF libraries. IDF targets also include CMake files, their runtime lock,
sdkconfig defaults, partition/model inputs and applicable local components.
Markdown documentation, website and test files are excluded from firmware keys.
Adding/deleting source files changes the key. Shared library changes rebuild
consumers; platformio.ini, the workflow and CI/packaging rules conservatively
invalidate all firmware. New IDF apps or nonstandard source filters must declare
their dependencies in the planner; it fails rather than silently omitting them.

Examples with the current dependency map:

- `apps/codex-monitor/src/main.cpp`: rebuild `codex-monitor` only.
- `lib/ui/src/ui/micro_layout.hpp`: rebuild `avatar-diag`, `codex-monitor` and
  the conservatively declared `codex-audio-diag` consumer.
- `docs/` or `site/` changes: reuse firmware; guide and host checks still run.
- `platformio.ini` changes: rebuild every environment.

The matrix uses four concurrent runners. Cache-hit jobs still restore and hand
images to the packaging job; the workflow never publishes a partial catalog.
Published bundles contain all boot/application/partition images, including
Arduino boot_app0 or monitor speech models. Build-only diagnostics cache a
successful-build marker and remain outside the browser catalog. No compiler
object files, installed toolchains or local private configuration are in these
firmware bundles. Download caches are shared by Arduino, standalone IDF and monitor runtime
families. Per-environment download caches were measured above 11 GiB on the first
run and evicted small firmware bundles; three shared caches avoid that duplication.

The packager's `--reuse-dir` mode verifies bundle contents and retains each
firmware's original build revision in its manifest. The catalog itself carries
the current website revision. GitHub's branch-scoped cache handles reuse across
runs; evicted images rebuild automatically. An invalid exact-key cache rebuilds
for correctness but must be deleted from Actions caches to replace that immutable
entry. Cache misses never cause affected firmware to be skipped.

`python scripts/build_firmware.py` still builds all apps locally. Inspect the CI
plan with `python scripts/firmware_ci.py plan`. Run planner/bundle/package tests
with `python -m unittest discover -s test/host`; validate workflow syntax with
`actionlint .github/workflows/*.yml`.

Validation for this change: 19 host tests pass, covering app/inherited/shared
changes, source additions/deletions, docs-only changes, complete catalog reuse,
original firmware versions and corrupt/missing bundle rejection. Actionlint
passes. The [first hosted run](https://github.com/cmwen/FNK0104B/actions/runs/37760063277)
completed successfully on commit `07e50b5`: all 28 firmware environments built,
all host checks passed, and the complete Pages catalog packaged from the
per-environment artifacts. The first documentation-only follow-up exposed download-cache duplication
and eviction; download caches were consolidated before repeating acceptance. Branch runs do not deploy to Pages.

Cache behavior follows the [official Actions cache contract](https://github.com/actions/cache/blob/main/README.md);
the output matrix follows [GitHub's matrix job documentation](https://docs.github.com/en/actions/how-tos/write-workflows/choose-what-workflows-do/run-job-variations).

## Independent guide publishing — 2026-10-10

The combined workflow is now split into firmware builds, guide publishing and
host checks. Guide/configuration/docs changes reuse the last complete successful
main firmware catalog without a firmware matrix. Firmware success independently
refreshes the published catalog. See [workflow triggers, artifact lifetime and
manual recovery](ci-workflows.md). The fingerprint planner now tracks only the
firmware workflow; changing the Pages workflow does not invalidate firmware.
