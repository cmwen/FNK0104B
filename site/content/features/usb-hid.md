---
title: "USB HID & a number-pad prototype"
summary: "Explore making the board appear as a keyboard to a computer or phone."
status: Explore
---

## Why this is possible
The board's Type-C data lines connect directly to the ESP32-S3 native USB pins. Espressif supports USB device classes through TinyUSB, including keyboard and mouse HID. This makes a touch number pad a plausible prototype. **No HID firmware or host compatibility test exists in this repository yet.**

HID means Human Interface Device. For a number pad the board acts as a USB device, describes itself as a keyboard and sends key press/release reports. The computer or phone is the USB host. The on-screen buttons are the local interface; they do not become a browser app on the host.

## Host expectations, not tested promises
| Host | Prototype expectation | What to verify |
| --- | --- | --- |
| Windows | Standard keyboard HID is a sensible starting point | Enumeration, number entry, Num Lock, layout and reconnect |
| macOS | Standard keyboard HID is a sensible starting point | Recognition, layout, key release and sleep/reconnect |
| Android | Phone must support USB host mode and the connection must put it in that role | OTG/adapter needs, power, keyboard handling and the target app |

These are design expectations from USB HID and host documentation, **not FNK0104B compatibility claims**. Android documents that host capability is not guaranteed on every device. A Type-C connector alone does not establish the correct cable role or power arrangement.

## USB changes affect the development connection
Existing apps use USB serial for flashing and diagnostics. The ESP32-S3 USB-OTG and USB Serial/JTAG controllers share a PHY, so using TinyUSB changes that path. Do not assume a HID build retains the same serial port or built-in JTAG access. HID plus CDC is an option to evaluate, not a tested configuration here. Plan and verify BOOT/RESET download recovery before relying on the new USB mode. No eFuse change is needed for this proposed experiment.

## A small first prototype
Start with one explicit touch button that sends a digit and then releases it. Verify no stuck key on cancel/disconnect. Then add digits, decimal, backspace and enter. Decide whether you want number-row digits or keypad usages, since keypad behavior can depend on Num Lock. Test your target host and app before building a polished keypad layout.

Keep the experiment in a new independently buildable PlatformIO environment, with USB hardware access in the shared board library. Do not combine it with the full monitor until its USB behavior is established.

## Brief your coding agent
> Prototype one touch-triggered USB HID digit and release in a new PlatformIO environment. Use verified native USB wiring and the supported TinyUSB stack. Document flashing recovery and test on my chosen host. Then build a keypad with clear pressed states and no stuck reports. Do not alter security settings.

## Sources
[Espressif USB device stack and HID example](https://docs.espressif.com/projects/esp-idf/en/v6.0/esp32s3/api-reference/peripherals/usb_device.html), [Android USB host overview](https://developer.android.com/develop/connectivity/usb/host), [Apple Core HID](https://developer.apple.com/documentation/corehid). Board wiring is verified separately in the repository knowledge files.
