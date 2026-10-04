---
title: "Expansion, buttons & remaining hardware"
summary: "Add physical controls while respecting the board\u2019s occupied pins."
status: Explore
---

## Known connections
The board exposes expansion GPIO and a shared I²C connector. A physical button on verified GPIO14 is already used by the button diagnostic and LocalLink speech. Expansion I²C shares the bus used by touch and the ES8311 codec.

An addressable RGB LED and battery ADC pin are declared in the shared definitions, but current apps have no initialization/read path for them. Battery measurement calibration, power runtime, external electrical limits and attached expansion devices remain **UNKNOWN**. Do not present these as working firmware features.

## Before attaching anything
Read the verified pin inventory. Occupied peripheral pins are not free merely because your current screen does not use them. Memory, USB and reset-strapping pins have restrictions. GPIO2 and GPIO21 are conditional candidates; connector orientation, voltage, current, loads and pulls must be checked before use. GPIO3 needs a reviewed strapping design.

## A useful pattern
A physical push-to-talk button can complement touch. First run `button-diag` with the documented wiring, then combine it with a tested microphone path. Debounce and define press/hold/release behavior before integrating a more complex application.

## Brief your coding agent
> Read knowledge/pins.yaml before proposing an expansion connection. Identify occupied buses and unknown electrical details. Build a small diagnostic first. Keep pin assignments and hardware access in lib/fnk0104b.
