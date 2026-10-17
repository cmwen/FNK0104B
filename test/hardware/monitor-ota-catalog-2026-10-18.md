# Monitor OTA catalog address — 18 October 2026

## Owner report

The owner reported that the setup page showed **Update catalog unavailable**
while asking whether 0.8.5 had been released. Installed version and serial-side
HTTP/TLS output were not provided at the start of this investigation. This is
an owner report, not an independently captured device test.

## Host and source evidence

- Firmware CI run `39034353821` and Pages publication completed successfully.
- A no-follow HTTPS GET to
  `https://cmwen.github.io/FNK0104B/firmware/codex-monitor/ota.json` returned
  HTTP 301 with Location
  `https://cmwen.dev/FNK0104B/firmware/codex-monitor/ota.json`.
- The canonical URL returned HTTP 200 with version `0.8.5`, application size
  3,407,232 bytes, SHA-256
  `bb8b649fe2ff8ff1440961cd9ef60a7e24d6eb36a47d00c5bc767dc2b92d711d`,
  layout `monitor-ota4m-model810000-v1`, and model SHA-256
  `e0a43266597da7db07f066a80893ca093f6a9395e9c298af3941eaf6d3c3e833`.
- The downloaded canonical `firmware.bin` matched the catalog size and SHA-256.
- Source in firmware up to 0.8.5 uses the GitHub Pages URL, disables HTTP
  redirects, and returns the catalog-unavailable message for any non-200 result.

## Correction and limits

0.8.6 changes the metadata and image base to the canonical HTTPS origin and adds
connection/header/HTTP-status diagnostics. The partition table, models, TLS
verification, hash/size validation and rollback policy are unchanged. An older
board that rejects the redirect needs a USB recovery install; no browser action
can rewrite its compiled URL.

No device upload, erase or security operation was performed in this investigation.
The redirect is a source-supported explanation; the exact board failure still
requires serial evidence to rule out a separate DNS/network/TLS issue. Successful
physical OTA, power interruption and rollback remain UNKNOWN.

## Automated checks

The PlatformIO `codex-monitor` 0.8.6 build passed. The packager confirmed version
0.8.6 and the same layout/model hash as published 0.8.5. All 44 host tests and
four guide tests passed. The responsive browser smoke check passed after the
usage timer preview and recovery link were added. These are build/host checks,
not evidence of a board download or boot.
