---
title: "Browser flashing & OTA"
summary: "Install independent firmware and understand what an update replaces."
status: Implemented
---

## Browser installation
The existing installer uses ESP Web Tools and **Web Serial** to flash over the USB cable. It is often called browser USB flashing, but this flow uses Web Serial rather than the WebUSB API. Use desktop Chrome/Edge on HTTPS or localhost and a USB data cable. Select the Espressif serial device and keep the tab open until writing completes.

Firmware manifests list the bootloader, partition table, application and, for the monitor, speech models at their configured offsets. Those parts are built with PlatformIO Core CLI in CI. The website does not compile firmware in the browser.

## Preserving settings
Leave the erase choice unchecked for compatible updates. An erase deletes saved settings. A changed partition layout can overwrite data even without erase-all: the speech monitor replaces OTA/FATFS space with models. Read each firmware's storage note before switching.

## Monitor wireless updates
Monitor 0.7.0 adds confirmed HTTPS OTA after a one-time full USB install. Connect
to monitor settings in Device setup, then use **Check for update** and **Install
update**. Bluetooth carries commands; the board downloads over saved Wi-Fi.
The microphone is off during update work. A changed speech-model image or
partition layout requires USB. Download failures keep the current app; failed
first boots can roll back to the previous valid app. Physical monitor OTA and
rollback verification are still pending.

The standalone `ota` demo remains a separate GitHub Release update example.
The recorder does not gain OTA from this monitor change.

## Local development
Use PlatformIO Core CLI to discover the device, build its named environment, upload, then monitor serial at 115200 baud. Arduino IDE is not required. Public firmware has no personal bridge key; a live Codex monitor needs your locally configured build.

## Brief your coding agent
> Package all required images using the existing PlatformIO workflow. Explain partition changes before installation and keep the erase choice explicit. Preserve saved settings only where the layout is compatible.
