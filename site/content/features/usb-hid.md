---
title: "USB emoji and number-pad keyboard"
summary: "Use the touchscreen as a wired USB keyboard, with number-pad keys and OS-specific emoji shortcuts."
status: Device-verified
---

## A touchscreen keyboard over USB
The board's Type-C data lines connect directly to the ESP32-S3 native USB pins. The `keyboard` firmware uses TinyUSB to register a standard HID keyboard plus USB serial. The touchscreen has number-pad and emoji pages; nothing is typed automatically at boot.

On 4 October 2026 the owner confirmed that touchscreen keyboard input works directly on Windows 11. Their earlier Windows App remote session did not receive the board's keystrokes as expected. Test on the computer directly first; this observation does not establish remote-session forwarding or exhaustive key coverage.

## Controls and host behavior
The number pad sends real HID keypad digits, decimal, arithmetic operators, Backspace, Tab, Enter and Escape. Tap **Num** until Num Lock is on for digit entry. The top-right button selects Windows, Mac or Linux; Windows is the startup default.

| Host mode | Emoji behavior | Evidence |
| --- | --- | --- |
| Windows | A favorite opens Win+period and types an English search term; choose the result on the PC or use Enter, then Escape | Direct touchscreen keyboard input works; emoji selection still needs a separate check |
| Mac | Opens the Character Viewer with Control+Command+Space for manual selection | Implemented, not host-tested |
| Linux | Sends Ctrl+Shift+U and hexadecimal Unicode input for compatible GTK text fields | Implemented, not host-tested; not universal across Linux apps |
| Android | No dedicated emoji mode; keyboard use also requires a supported USB host connection | Not tested |

The display uses readable favorite labels because its current fonts lack emoji glyphs. Search/hex input assumes a US-compatible keyboard layout, and Windows searches are English. HID sends key events, not clipboard text or Unicode directly.

## Build and flashing
Use the named PlatformIO Core CLI environments:

```sh
pio run -e hid-diag -e keyboard
pio run -e keyboard -t upload
pio device list
pio device monitor -b 115200
```

`hid-diag` isolates HID plus CDC before the combined touch app. The shared hardware implementation lives in `lib/fnk0104b`; no new GPIOs were assigned.

Both use the existing Arduino partition profile. Uploading from the monitor/speech/recorder layout replaces the app and partition table; previous flash model/data regions may become inaccessible or be overwritten. SD files are untouched. Restoring the monitor means uploading its named environment and speech models.

## USB access and recovery
USB-OTG and USB Serial/JTAG share a PHY, so built-in USB-JTAG is unavailable while this firmware uses TinyUSB. USB serial remains available at 115200 baud. A PlatformIO 1200-baud reset into ROM download mode was verified; physical BOOT/RESET recovery still needs a separate check. No eFuse or security change is needed.

On WSL, USB/IP can detach during the identity change between firmware and ROM. Reattach the shared board and rediscover its serial port before retrying. After keyboard flashing and monitoring, detach it from WSL so Windows owns the keyboard; a WSL-owned USB device cannot type directly into Windows apps.

## Evidence and remaining checks
[The keyboard README](https://github.com/cmwen/FNK0104B/blob/main/apps/23-keyboard/README.md) documents controls, flash effects and limitations. [The dated hardware record](https://github.com/cmwen/FNK0104B/blob/main/test/hardware/hid-keyboard-2026-10-04.md) separates verified builds/uploads, serial and enumeration evidence, and the owner's direct Windows 11 test from remaining emoji, Num Lock, reconnect and sustained-use checks.

## Sources
[Espressif Arduino USB API](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/usb.html), [Espressif USB device stack](https://docs.espressif.com/projects/esp-idf/en/v6.0/esp32s3/api-reference/peripherals/usb_device.html), and [Android USB host overview](https://developer.android.com/develop/connectivity/usb/host). Board wiring is verified separately in the repository knowledge files.
