# FNK0104B OTA learning diagnostic

This app demonstrates a user-confirmed HTTPS firmware update from a public
GitHub Release. It reuses Wi-Fi credentials already saved in the board's NVS,
checks the latest release after connecting, and reports the installed and
available versions in the 115200-baud serial monitor. Send `y` to install or
`n` to skip. It prints download progress and reboots into the new image.

The URL is the latest non-draft GitHub Release asset named `ota.bin`:

```text
https://github.com/cmwen/FNK0104B/releases/latest/download/ota.bin
```

The repository's existing 16 MB partition layout already has two 3 MiB OTA
slots and OTA metadata; this app does not change the partition table. The
update is written to the inactive slot. The bootloader has application rollback
enabled, and the app marks itself valid after startup. If a new image fails to
boot, the bootloader can return to the previous image. The current running
firmware remains available in the other slot during the download.

## Try the update

1. Save Wi-Fi credentials to the board with `wifi-ble` or `connectivity`.
2. Build and install this app over USB:

   ```sh
   pio run -e ota
   pio run -e ota -t upload
   ```

3. Open `pio device monitor -b 115200` and wait for the latest release check.
   If a newer tag is available, the monitor prints both versions and asks for
   confirmation. Send `y` followed by Enter to install or `n` to skip. Send `c`
   to check again later.
4. Watch the clock sync, download progress, and result messages. A successful
   update reboots automatically and prints its firmware version on startup.

TLS certificate verification is enabled with the Mozilla root bundle included
in the pinned ESP32 framework, so the app can validate GitHub's HTTPS API and
release download redirects.

## Publish an update

For a learning run, first install one OTA firmware build over USB. Then change
`kFirmwareVersion` in `src/main.cpp`, build again, and create a GitHub Release
with a newer tag. Attach that build's `.pio/build/ota/firmware.bin` file with
the exact name `ota.bin`. The board will report the new tag and wait for `y`;
the new build should download to the other slot and boot after restart. Keep
each firmware image under the 3 MiB app-slot limit.

GitHub Releases are a good fit for this demo because firmware assets are public
and a release tag gives each build a human-readable version. The current CI
builds firmware and deploys the USB browser flasher; it does not create GitHub
Releases or attach OTA assets. Publishing a release is a separate manual step.

The current confirmation prompt is serial-based. The repository has a separate
touch diagnostic; after touch is verified on the physical board, the prompt
could move onto the display. This demo trusts GitHub's TLS certificates and
downloads the latest release asset. It does not yet verify a firmware signing
key or provide a release selection screen. Do not use it as a production
updater for devices that need signed firmware policy or staged rollout
controls.
