# Emoji and number-pad USB keyboard

```sh
pio run -e hid-diag -e keyboard
pio run -e keyboard -t upload
pio device list
pio device monitor -b 115200
```

Connect the Type-C port to your computer with a data cable, focus the app where
you want to type, and tap the screen. This is a wired USB HID keyboard.

- **Numpad:** 0–9, decimal, arithmetic operators, Enter, Backspace, Tab and Esc.
  These are actual HID keypad keys. Tap **Num** until the host reports
  **Num Lock ON** for digits; when off, the host may treat them as navigation.
  The firmware does not change Num Lock automatically.
- **Emoji:** twelve labeled favorites plus picker, Enter and Esc controls.
- **Windows/Mac/Linux:** tap the top-right button to cycle the host mode. The
  startup default is Windows; selection is kept until reset.
- Touches send once per press. Nothing is typed at boot or while disconnected.

Emoji behavior is explicit because HID sends key events rather than Unicode:

| Host mode | Emoji behavior |
|---|---|
| Windows | Opens Win+period, waits 650 ms and types the favorite's English search term. Choose the result on the PC or use Enter, then Esc. Search focus, timing, language and result order require host validation. |
| Mac | Opens Control+Command+Space Character Viewer. Choose the emoji on the Mac; the favorite buttons currently all open this viewer. No automatic favorite insertion. |
| Linux | Sends Ctrl+Shift+U, hexadecimal codepoint and Enter. Compatible GTK text fields can insert favorites directly; this is not supported by every Linux app or input method. Heart also sends U+FE0F for emoji presentation. |

ASCII search/hex input assumes a US-compatible host keyboard layout. Emoji
searches are English. Focus must remain in the intended app; firmware cannot
inspect or confirm the host's picker state. An unsupported shortcut may type
text into that app. Favorites use readable labels because the current TFT
fonts do not include emoji glyphs.

## USB, flashing and recovery

The HID environments enable PlatformIO's 1200-baud open/close reset and wait
for the upload port. This enters the pinned Arduino TinyUSB bootloader path.
On WSL, USB/IP can detach during that transition. If port discovery fails,
reattach the already shared board with `usbipd attach --wsl --busid <busid>`,
rediscover the port, and retry the normal PlatformIO upload. A ROM reset via
this path was verified; manual BOOT/RESET recovery remains to be checked.

The shared implementation lives in `lib/fnk0104b`, with the existing display
and touch APIs. USB uses the verified native Type-C wiring without assigning
new GPIOs. Both environments register TinyUSB HID plus CDC and disable the
board profile's Serial/JTAG USB mode. Serial remains at 115200 baud. USB serial
VID/PID and port name can change after upload; rediscover the device.

Both inherit the existing Arduino `app3M_fat9M_16MB.csv` partition layout.
Uploading from the IDF monitor/speech/recorder replaces its partition table and
firmware; previous model/data regions may be overwritten or inaccessible.
There is no full-chip erase, eFuse change, or SD-card access. To return to the
monitor/recorder, upload its own named environment and required models.

For download recovery, hold BOOT, press/release RESET, then release BOOT and
run PlatformIO discovery/upload on the ROM port. Built-in USB-JTAG cannot run
while TinyUSB uses the shared internal PHY.

On WSL, a board attached through USB/IP belongs to Linux. After flashing and
closing the monitor, detach the device from WSL in PowerShell with
`usbipd detach --busid <actual-busid>` (obtain it from `usbipd list`) and stop
any auto-attach loop so Windows can enumerate the keyboard locally. Firmware
cannot make a WSL-owned USB device type directly into Windows applications.

Test on the Windows computer directly before testing a remote desktop session.
On 2026-10-04 the owner confirmed touchscreen keyboard input works directly
on Windows 11. Their Windows App remote session did not deliver the board's
keystrokes as expected. Remote-session forwarding in that setup is unverified;
the initial missing-input reports did not establish a local HID failure.

## Evidence

Implementation and build/device results are tracked in
[the HID hardware checklist](../../test/hardware/hid-keyboard-2026-10-04.md).
Direct Windows 11 touchscreen keyboard input is owner-verified. Emoji
selection, individual key/Num Lock coverage, other operating systems, remote
session forwarding and sustained HID behavior remain UNKNOWN until tested.

Version 0.1.1 corrects a connection-status bug: TinyUSB endpoint readiness
temporarily becomes false during a report and must not be used as USB
connection state. The app now uses mounted/non-suspended USB state for its
connection indicator, waits up to 250 ms for the endpoint before each report,
and sends no unsolicited release reports on connection/Num Lock updates.
Serial logs include touch coordinates, endpoint readiness and transfer errors.
Sending `t` over serial deliberately tests typing `123` into the focused host
app; do not use that command without first focusing an empty text field.
The USB product string is set at compile time before USB starts.

## Primary references

- [Espressif Arduino USB API](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/usb.html), cross-checked against the installed pinned Arduino 2.0.17 USB sources.
- [Espressif USB device stack](https://docs.espressif.com/projects/esp-idf/en/v6.0/esp32s3/api-reference/peripherals/usb_device.html), chip capability and shared PHY restriction.
- [Microsoft emoji input](https://support.microsoft.com/en-US/Windows/Hardware/Input-Devices/windows-keyboard-tips-and-tricks).
- [Apple keyboard shortcuts](https://support.apple.com/en-us/102650).
- [GNOME Unicode entry](https://help.gnome.org/gnome-help/tips-specialchars.html).
