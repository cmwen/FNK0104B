# Build, upload, and serial monitor

## Build

```bash
pio run                  # default hello firmware
pio run -e hello
pio run -e display       # current placeholder, for example
pio test -e native       # host test; no board required
```

Build the selected firmware before uploading it.

## Upload

Connect the FNK0104B with a USB data cable and identify its serial device. Then run:

```bash
pio run -e hello -t upload
```

To select a port explicitly:

```bash
pio run -e hello -t upload --upload-port /dev/ttyACM0
```

PlatformIO is configured for 16 MB flash, QIO at 80 MHz with OPI PSRAM, USB CDC on boot, and 460800 baud upload. The partition profile matches the 16 MB / 3 MB app / 9.9 MB FATFS setting shown in Freenove's tutorial and reserves two OTA app slots. The factory partition table has not been read from this physical board. Upload writes the selected table along with bootloader/application files; review `docs/memory.md` before the first upload if existing on-flash data must be preserved. The configuration does not request an erase-all operation.

## Serial monitor

```bash
pio device monitor -b 115200
```

The `hello` app waits up to three seconds for USB CDC, then prints the firmware/version, detected chip model, detected flash size, PSRAM size when initialized, free heap, and a running status. Reset the board while the monitor is open if the startup lines have already passed.

## Hardware verification log

On 2026-09-27, PlatformIO uploaded `hello` successfully to `/dev/ttyACM0`; esptool verified the hashes after writing the bootloader, selected partition table, and application. Esptool identified an ESP32-S3 QFN56 revision v0.2 with embedded 8 MB PSRAM. The serial monitor repeatedly received `status=running`. A reset yielded a partial startup capture with `psram_bytes=8386295` and `free_heap_bytes=371116`; USB detached during reset, so the firmware/version lines and full flash-size line were lost. Reattach with `usbipd` after resets if WSL no longer lists the serial port, then open the monitor before another reset to capture the complete banner.

After updating the PlatformIO platform pin to 7.0.1, a second `hello` upload succeeded with esptool 4.11.0 and verified all written hashes. `/dev/ttyACM0` reappeared after reset and stayed present through a 30-second WSL USB poll, but several monitor captures initially received no firmware text. A later user capture received `status=running`, then the USB connection dropped with `Input/output error`. The WSL kernel logged a USB/IP peer reset and later reattachments. After reattachment, this same board enumerated as `/dev/ttyACM1`; retrying the old `/dev/ttyACM0` name failed because that node no longer existed. Monitoring the persistent `/dev/serial/by-id/` link then received four `status=running` lines over roughly 30 seconds with no further disconnect. The complete startup banner still needs capture after a reset.

## WSL2 USB passthrough

At initial setup, WSL saw the board as Espressif USB JTAG/serial (`303a:1001`) on `/dev/ttyACM0`. PlatformIO listed it as `USB JTAG/serial debug unit`. After adding `cmwen` to `dialout` and refreshing the shell's group credentials with `newgrp dialout`, the upload succeeded. If serial access fails with permission denied, check group membership and refresh the login shell.

After attaching the device, check access with:

```bash
pio device list
ls -l /dev/ttyACM0
id -nG
```

The ACM number can change after a USB/IP detach and reattach. This board also has a stable udev path on this WSL host:

```bash
ls -l /dev/serial/by-id/
sg dialout -c 'pio device monitor -p /dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_B8:1F:3F:C3:9F:94-if00 -b 115200'
```

That link currently points to `/dev/ttyACM1`. It is specific to the connected board's USB serial number; use `pio device list` or `ls -l /dev/serial/by-id/` if another board is attached. PlatformIO can open the link and has received repeated hello heartbeats through it. If the firmware resets and auto-attach restores the device under a new ACM number, a monitor using the link has a consistent path to retry.

On this machine, grant the login account serial-port group access from WSL:

```bash
sudo usermod -aG dialout "$USER"
```

Then start a fresh login shell (or run `newgrp dialout`) so the new supplementary group is active. Confirm `dialout` appears in `id -nG`, then build/upload and monitor as shown above. This changes ordinary Linux device access only; it does not alter the board.

USB attachment to WSL is non-persistent. Manual attachment must be repeated after WSL restarts, device resets, or unplug/replug; a running `usbipd attach --wsl --busid <BUSID> --auto-attach` loop can reattach automatically after resets. Keep that PowerShell loop open. If the board is not visible in WSL, list devices in PowerShell running as Administrator. Bind it only if its state is `Not shared`:

```powershell
usbipd list
usbipd bind --busid <BUSID>
```

From normal PowerShell, attach it to WSL:

```powershell
usbipd attach --wsl --busid <BUSID>
```

Then in WSL:

```bash
pio device list
find /dev -maxdepth 1 \( -name 'ttyACM*' -o -name 'ttyUSB*' \) -print
```

`usbutils` (`lsusb`) is not installed in this Ubuntu image. Use `pio device list` to identify serial ports without it, or install `usbutils` with `sudo apt install usbutils` when administrator credentials are available. Do not use a guessed port; use the device that appears after attaching the board.

## Recovery with BOOT/download mode

If an application does not boot, use the board's BOOT and RESET buttons to force the ROM download mode. Freenove's module guide describes either holding BOOT while powering the board, or holding BOOT, pressing and releasing RESET, then releasing BOOT. Reconnect/check the serial port and retry the normal PlatformIO upload. This procedure does not require eFuse or security-setting changes.

## Future hardware tests

The staged device checks are listed in `test/hardware/README.md`. Keep one capability per diagnostic app and capture the serial log after every upload.
