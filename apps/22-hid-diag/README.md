# USB HID diagnostic

Build with `pio run -e hid-diag`; upload with `pio run -e hid-diag -t upload`.
Inspect `pio device monitor -b 115200` after upload. Discover the current port
with `pio device list` because USB identity changes.

The board registers a standard keyboard plus USB CDC. It never types at boot.
Sending `t` over serial types `123` into the host's focused app; deliberately
focus an empty text field before using this test. Every two seconds it reports
HID readiness and the host's Num Lock LED. No display/touch initialization.

Uses the pinned Arduino 2.0.17 USB implementation through PlatformIO, selecting
`ARDUINO_USB_MODE=0` instead of the board profile's Serial/JTAG mode. Physical
USB wiring stays in the existing board definition; no GPIO reassignment.
USB-JTAG is unavailable while TinyUSB owns the internal PHY. Recover using
BOOT held, press/release RESET, release BOOT, then discover the ROM serial port
and upload normally. Recovery on this specific HID firmware is UNKNOWN until
tested on hardware; no security settings are changed.

Flash effect: inherits the existing Arduino `app3M_fat9M_16MB.csv` layout.
Uploading from a monitor/speech/recorder IDF layout replaces its partition table
and app; previous model/data regions may be overwritten or inaccessible. SD
files are untouched. No full-chip erase is requested. See the keyboard README
for host and USB/IP limitations and verification evidence.
