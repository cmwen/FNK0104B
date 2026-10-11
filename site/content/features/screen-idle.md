---
title: "Screen inactivity & wake"
summary: "Make an always-on appliance quieter without losing its connection."
status: Implemented
---

## Screen off is not deep sleep
The implemented pattern switches the active-high backlight fully off after inactivity while the CPU continues running. Touch remains polled, Wi-Fi can remain connected, and the monitor can still receive updates and listen for its wake phrase. This is not measured low-power sleep.

Deep sleep suspends normal application work and needs a verified wake source and a restoration design. It is not implemented by the current screen timeout feature. No power-saving percentage has been measured.

## Existing behavior
`screen-timeout` turns the backlight off after sixty seconds without touch and wakes on touch. It does not implement PWM dimming. The monitor exposes idle timeout over BLE, supports quiet mode, and has local voice commands for screen on/off. Confirm the current allowed timeout values in the setup page.

## Optional idle photos
Monitor 0.8.0 lets you keep normal controls, blank the area below the usage bar,
or show SD photos. The live bar stays visible. Set an idle delay and photo
interval in Device setup → Monitor settings. For example, photos after 60
seconds and screen off after 10 minutes give nine minutes of photos; changing
photos does not restart that countdown. Activity restores the controls.

The browser crops and resizes JPEG/PNG/WebP before upload. Photo mode defaults
off. SD writes and LCD rendering were checked in USB Micro mode; physical BLE
configuration/upload, touch/automatic idle timing and Wi-Fi speech coexistence
still need testing. Read [the setup and evidence limits](https://github.com/cmwen/FNK0104B/blob/main/docs/monitor-photos.md).

## Useful design choices
Define what counts as activity: a tap, a voice command or a status change? Decide whether the first wake tap only wakes the screen or also activates a control. Keep touch detection alive while dark. Decide how an alert interacts with quiet mode, and make saved preferences survive compatible updates.

## Brief your coding agent
> Add inactivity-based backlight off using the existing board API. Keep touch and background status alive. Specify first-tap behavior and quiet-mode interactions, persist the timeout in NVS, and verify wake on the physical device.
