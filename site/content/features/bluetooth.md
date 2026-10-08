---
title: "Bluetooth LE & Web Bluetooth"
summary: "Configure a powered board from a browser, without another firmware upload."
status: Implemented
---

## What BLE does here
The ESP32-S3 provides Bluetooth Low Energy. This is a way to exchange small settings or control messages, not a Bluetooth Classic audio speaker. Web Bluetooth lets a supported browser connect to a BLE GATT service after you select the device.

## Two different connections
**Wi-Fi provisioning** advertises `FNK0104B-SETUP`. Enter the fresh code displayed on the board, then send a 2.4 GHz network name and password using Espressif Security 1. Successful credentials are stored on the board. The website does not save the password.

**Monitor settings** advertise `FNK0104B-MONITOR` during normal operation. The browser changes alert volume, idle screen timeout and USB Micro layout (three agent slots plus four directions by default, or six slots). The monitor reserves BLE for settings; Micro control uses USB HID. This connection does not configure the private bridge host or key, and successful BLE setup does not prove the bridge is reachable.

The monitor enters its separate Wi-Fi setup boot automatically when credentials are missing. Hold its Wi-Fi indicator for three seconds to request setup. Normal monitor settings BLE is unavailable during this setup mode. Separate names make the two workflows easier to explain.

## Browser and platform expectations
Use desktop Chrome on macOS, or Chrome/Edge on Windows, over HTTPS or localhost, with Bluetooth enabled. Choose the device in the site's picker; OS-level pairing is not required. Safari and Firefox cannot use these controls. Chrome documents Web Bluetooth on Android; this project's Android provisioning flow is **UNKNOWN** until tested. Linux support is dependent on browser configuration. Only one settings client should connect at a time.

## Start here
Use `wifi-ble` for the provisioning diagnostic and `connectivity` for BLE scanning/advertising. Close the previous connected browser tab before switching computers.

## Brief your coding agent
> Add a browser-editable preference through the existing BLE settings service. Explain connection, validation, saved state and reconnection. Never print credentials. Test the firmware diagnostic and browser behavior separately.

## Sources
[Chrome Web Bluetooth documentation](https://developer.chrome.com/docs/capabilities/bluetooth). Board provisioning details come from the repository's Wi-Fi over BLE README and monitor readiness record.
