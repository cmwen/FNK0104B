# FNK0104B OTA learning diagnostic

This app demonstrates a user-confirmed HTTPS firmware update from a public
GitHub Release. It reuses Wi-Fi credentials already saved in the board's NVS,
checks the latest release after connecting, and reports the installed and
available versions on the touchscreen. Tap **INSTALL** to update or **LATER**
to skip. The display shows download progress and the device reboots into the
new image. Serial commands remain available for diagnosis: `c` checks, `y`
installs, and `n` skips.

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

3. The screen shows the installed and latest versions. Tap **INSTALL** to
   update, **LATER** to skip, or **CHECK AGAIN** to retry a release check.
4. Open `pio device monitor -b 115200` for Wi-Fi, touch-coordinate, and OTA
   status messages. A successful update reboots and prints its version there.

The OTA environment uses the shared FNK0104B ILI9341 setup from
`lib/fnk0104b/src/tft_setup.h` and `fnk0104b::display.begin(1)`: landscape
320×240 orientation, verified board pins, active-high backlight, and the
panel's verified `INVON` color setting. Touch coordinates use the matching
landscape transform in the shared board support. On first hardware check,
confirm `touch=ready` and tap each button while watching the serial output.

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

The repository also has a standalone touch diagnostic for checking the panel
before using this combined app. This demo trusts GitHub's TLS certificates and
downloads the latest release asset. It does not yet verify a firmware signing
key or provide a release selection screen. Do not use it as a production
updater for devices that need signed firmware policy or staged rollout
controls.
