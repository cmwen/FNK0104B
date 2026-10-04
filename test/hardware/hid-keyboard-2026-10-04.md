# HID keyboard verification — 2026-10-04

New `hid-diag` and `keyboard` environments use the pinned Arduino toolchain,
TinyUSB HID plus CDC, existing verified USB wiring and Arduino partition table.
No full-chip erase or security changes are requested.

## Completed

- `pio run -e hid-diag -e keyboard`: both passed on platform 7.0.1 / Arduino
  2.0.17. Diagnostic: 321,397 bytes flash / 31,328 bytes RAM; keyboard:
  368,433 bytes flash / 31,624 bytes RAM.
- `pio test -e native`: all 21 tests passed, including touchscreen bounds/gaps.
- `python3 -m unittest discover -s test/host`: all 5 passed.
  Firmware catalog discovery separately confirmed both new environments.
  `git diff --check` passed.
- PlatformIO discovered the known board at `/dev/ttyACM0` (`303a:1001`, serial
  `B8:1F:3F:C3:9F:94`). The diagnostic upload wrote bootloader, Arduino
  partitions, boot-app metadata and app, verifying every hash. The flash
  regions were 0x00000000–0x00003fff, 0x00008000–0x00008fff,
  0x0000e000–0x0000ffff and 0x00010000–0x0005efff. No full-chip erase.
- After reset, USB/IP detached during the USB identity change. Esptool's
  post-write reset/reopen failed because `/dev/ttyACM0` disappeared; the overall
  upload command exited with failure despite verified writes. PlatformIO
  discovery then listed no devices. Serial could not be captured at 115200
  baud. Reattachment is required before installing the combined keyboard.
- Windows `usbipd list` subsequently reported bus `3-1`, VID/PID `303a:1001`,
  `USB Input Device, USB Serial Device (COM8)`, state `Not shared`. This
  establishes Windows composite enumeration, not working key reports or
  Linux serial access. `usbipd bind --busid 3-1` returned “Access denied;
  this operation requires administrator privileges.” User reattachment was
  requested; no administrator privilege change was attempted.

- After user reattachment, `pio device list` discovered TinyUSB CDC on
  `/dev/ttyACM0`, serial `B81F3FC39F94`. A bounded PlatformIO monitor at 115200
  baud received five `hid_ready=1 num_lock=0` diagnostic heartbeats.
- Initial keyboard upload could not enter ROM through the default reset
  sequence. The installed pinned Arduino CDC source documents 1200-baud
  bootloader restart, and PlatformIO supports it through board upload options;
  both HID environments now enable `use_1200bps_touch` and
  `wait_for_upload_port`. This entered ROM without physical buttons. A
  transient PySerial enumeration error occurred during USB/IP detachment;
  reattaching the already shared ROM device and retrying succeeded.
- `pio run -e keyboard -t upload --upload-port /dev/ttyACM0` rebuilt successfully,
  verified bootloader/partition/boot-app/app hashes and exited SUCCESS. The app
  region was 0x00010000–0x0006afff; the Arduino partition layout is unchanged
  from the diagnostic. No full-chip erase or security changes.
- After automatic USB/IP reattachment, the 115200-baud PlatformIO monitor
  received four `hid_ready=0 num_lock=0 touch_ready=1 host=Windows` heartbeats.
  Linux kernel logs registered HID v1.11 Keyboard (`hid-generic`) plus CDC ACM;
  USB configuration was 1 and power state active. Actual report readiness
  on the intended Windows host still needs checking. Opening the Linux input
  device for passive polling was denied by device permissions; passwordless
  sudo is unavailable. No host input permissions were changed.
- Closed the monitor and detached bus `3-1` from WSL. Windows then listed
  `USB Input Device, USB Serial Device (COM8)` as `Shared`, not `Attached`,
  returning the keyboard to Windows for user testing.

## Version 0.1.1 correction

The user initially reported Windows 11 recognizes the keyboard but screen taps
produced no visible keystrokes. They later explained this test used a Windows
App remote session. Local Windows input on version 0.1.0 was not separately
verified; enumeration must not be treated as successful input delivery.

Source review found the app used `USBHID::ready()` (endpoint idle/readiness)
as connection state and sent extra all-release reports on status changes.
TinyUSB's readiness check includes endpoint busy state. Version 0.1.1 uses
`tud_ready()` for mounted/non-suspended connection state, bounds endpoint
waiting to 250 ms per report, removes automatic status-change releases, and
logs touch coordinates and transfer failures. It also sets `USB_PRODUCT`
before USB initialization, replacing the generic profile label.

- Both `hid-diag` and `keyboard` builds passed. Keyboard uses 368,845 bytes
  flash and 31,624 bytes RAM. Platform version/partitions/pins unchanged.
- Keyboard 0.1.1 uploaded successfully with all image hashes verified. USB/IP
  again required automatic reattachment between TinyUSB and ROM mode.
- A 12-second PlatformIO monitor at 115200 baud captured four heartbeats:
  `hid_ready=1 endpoint_ready=1 num_lock=0 touch_ready=1 host=Windows`.
  This establishes ready state after the correction; it does not establish
  Windows key delivery.
- Closed the monitor and detached bus `3-1` to return the board to Windows;
  requested a focused Notepad 1/2/3 test from the user.

## Windows follow-up on version 0.1.1

The user initially reported the screen shows USB ready and Key sent for taps,
but no visible keystrokes through their Windows App remote session. Source
review confirms report format and usages match
the pinned USBHID keyboard implementation; transfer success still does not
prove application input delivery.

- Windows PnP reports OK for the board's HID Keyboard Device (`kbdhid`), USB
  Input Device, USB serial COM8 and composite parent. No per-device upper or
  lower filter was reported. USB/IP shows Shared rather than Attached, so
  Windows owns the device.
- Read-only keyboard accessibility registry inspection returned Flags=126,
  DelayBeforeAcceptance=1000, AutoRepeatDelay=1000, AutoRepeatRate=500,
  BounceTime=0. No settings were changed.
- Two bounded Windows Raw Input captures filtered to this board's VID/PID
  observed zero events. Physical taps during these windows were not confirmed,
  so this does not establish that firmware reports were lost.
- Requested a focused empty-Notepad Num Lock toggle and Enter/new-line test
  to separate numeric keypad navigation/focus from report delivery.

## Owner-confirmed direct Windows 11 input

On 2026-10-04 the owner confirmed the board keyboard works when using Windows
11 directly. Their earlier tests used a Windows App remote connection, where
the HID input did not reach the session as expected. This establishes working
touchscreen-to-local-Windows keyboard input on the installed version 0.1.1;
it does not establish remote-session forwarding or exhaustive key coverage.
No further firmware change was needed after this clarification. The owner
requested documentation, commit/push, then restoration of the Codex monitor.

## Remaining checks (UNKNOWN)

- Exhaustive keypad/operator coverage and Num Lock LED feedback on Windows.
- BOOT/RESET ROM recovery.
- Physical touch layout, one event per tap, digit/operators/Enter behavior.
- Windows picker search and insertion on intended PC/layout/language.
- macOS Character Viewer and Linux GTK Unicode insertion.
- Unplug/replug, no stuck modifiers, sustained use.
