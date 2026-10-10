# Codex monitor wireless updates (0.7.1+)

Monitor 0.7.0 adds app-only HTTPS OTA. Bluetooth carries check/install commands;
firmware downloads use the board's saved Wi-Fi connection. This works alongside
USB Micro mode and does not need the local Codex bridge. Updates are manual;
connecting or checking never installs firmware automatically.

## One-time USB migration

Install **Codex monitor 0.7.1 or newer** using the browser USB installer or
PlatformIO. Earlier monitor versions have a single factory app and cannot acquire
OTA through Bluetooth alone. If you built an earlier monitor locally, first back
up or remove the generated `sdkconfig.codex-monitor` file so PlatformIO applies
the current defaults. For example, move it to `sdkconfig.codex-monitor.pre-ota`;
the file is regenerated on the next build. The build rejects disabled rollback
or an internal-only TLS allocator, since old generated settings can override
updated defaults. This changes build configuration, not saved board preferences.
Build before upload:

```sh
pio run -e codex-monitor
pio device list
pio run -e codex-monitor -t upload --upload-port <port>
pio device monitor --port <port> -b 115200
```

The full USB upload includes bootloader, partition table, initial OTA metadata,
application and speech models. Keep **Erase device** unchecked in the browser.
NVS remains at `0x9000`, size `0x5000`, preserving compatible saved Wi-Fi and monitor
preferences. Switching from the standalone speech/recorder NVS layout does not
guarantee preservation. Back up any old filesystem data you need.

The migration changes the partition table:

| Partition | Offset | Size | Purpose |
|---|---:|---:|---|
| nvs | 0x9000 | 0x5000 | Existing preferences and Wi-Fi |
| otadata | 0xe000 | 0x2000 | Boot slot selection, initialized erased on USB install |
| ota_0 | 0x10000 | 0x400000 | First 4 MiB application slot |
| ota_1 | 0x410000 | 0x400000 | Second 4 MiB application slot |
| model | 0x810000 | 0x7f0000 | Speech models |

This replaces the old 6 MiB factory app, PHY partition and model region. Old app,
model and filesystem data may be overwritten. No erase-all, eFuse, Secure Boot,
Flash Encryption or eFuse anti-rollback changes are involved. Moving the model
partition requires the full USB upload; sending only `firmware.bin` is insufficient.

## Use wireless updates

1. Keep the board powered. Configure Wi-Fi through Device setup if needed.
2. Open [Device setup](https://cmwen.github.io/FNK0104B/setup.html#setup) in Chrome
   or Edge, click **Connect to monitor**, and select the board.
3. Finish dictation or voice chat. Click **Check for update** under Firmware
   updates. The page shows installed/published versions and compatibility.
4. When a compatible newer version is available, click **Install update**.
   Keep power connected while it downloads and restarts. Reconnect to verify
   the installed version. Bluetooth disconnection alone does not prove success.

Wireless updates install the public CI monitor build. Saved Wi-Fi and board
preferences remain in NVS, but a private bridge host/key compiled into a custom
firmware is not retained. If you depend on that custom local bridge, build and
install your customized firmware over USB instead. USB Desktop mode needs no
private bridge configuration.

The board microphone is off during checks and installation and stays closed
when work finishes. Normal Hold to talk/Voice controls resume afterward. Existing
preferences remain unchanged. If firmware 0.7.0+ is installed but update controls
are unavailable, restart the board, refresh the page and reconnect. If needed,
remove a saved OS Bluetooth pairing to clear stale cached services.

For serial diagnostics, `ota-check` requests a check; `ota-install` installs only
a previously checked compatible update. Logs use the `monitor_ota` prefix.

## Compatibility and recovery

GitHub Pages publishes `firmware/codex-monitor/ota.json` from the actual compiled
application descriptor and partition table, alongside the application image.
The updater accepts newer numeric versions only, verifies HTTPS certificates
using the IDF certificate bundle and requires a synchronized clock. It verifies
exact downloaded size/SHA-256 and the IDF image before selecting the inactive
slot. It compares the installed speech-model bytes with the published model hash.
Monitor 0.7.1 sorts the speech model and file entries into a canonical bundle:
the upstream packer's filesystem order otherwise produced different hashes for
identical weights on CI and local builds. Upgrading a 0.7.0 board requires one
more full USB install to establish this canonical model baseline, even though
the model contents are unchanged. Future identical model sets produce identical
hashes. A changed model or layout requires USB; OTA never writes bootloader, partitions,
NVS or speech models. A release that changes between check and install requires
another explicit install action.

A failed or interrupted download leaves the running slot selected. The bootloader
has application rollback enabled: an OTA boot stays pending until core startup
(audio input and required mutexes) succeeds and the main loop runs for ten
seconds after setup. A restart/crash before confirmation causes rollback to the
previous valid app. Wi-Fi and the bridge are not prerequisites for confirmation.
A first USB install has no previous OTA application to roll back to. A failure
that does not restart the board still requires a reset or USB recovery.

For future releases, bump `monitor_ota::version` in `apps/codex-monitor/include/monitor_ota.hpp`.
CMake derives the IDF image version from that same declaration. A PlatformIO
pre-build hook detects version changes in cached CMake metadata and forces
reconfiguration, because its normal dependency check ignores header changes; unrelated firmware
keeps its cache keys. OTA does not replace same-version builds; use USB for
those. Firmware CI publishes the application and metadata through the existing
firmware catalog. Documentation-only publishing reuses that verified catalog
and does not rebuild firmware.

## Evidence and open hardware checks

Host checks cover version/downgrade rules, update-page gating and firmware
metadata/partition compatibility. Both affected builds, the USB migration and
post-flash OTA request handling are recorded in [monitor readiness](monitor-readiness.md).
The updater reuses the idle voice worker because the physical board did not have
enough internal RAM for an additional task alongside speech.
Monitor OTA download, power interruption and boot rollback on the physical board
are **UNKNOWN** until those checks are recorded. The standalone OTA demo's earlier
hardware success does not establish monitor behavior.

References: [ESP-IDF OTA and rollback](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32s3/api-reference/system/ota.html),
[ESP-IDF HTTP client](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32s3/api-reference/protocols/esp_http_client.html).
