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

## Useful design choices
Define what counts as activity: a tap, a voice command or a status change? Decide whether the first wake tap only wakes the screen or also activates a control. Keep touch detection alive while dark. Decide how an alert interacts with quiet mode, and make saved preferences survive compatible updates.

## Brief your coding agent
> Add inactivity-based backlight off using the existing board API. Keep touch and background status alive. Specify first-tap behavior and quiet-mode interactions, persist the timeout in NVS, and verify wake on the physical device.
