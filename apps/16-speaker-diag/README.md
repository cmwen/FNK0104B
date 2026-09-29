# FNK0104B speaker diagnostic

This standalone PlatformIO target shows a one-octave touchscreen piano from
C4 to C5. Touch and hold a key to play it; release to silence. It configures
the onboard ES8311 DAC and speaker amplifier. Connect a speaker to the board's
PH1.25 speaker connector; Freenove documents the connector, not a built-in
speaker transducer. The amplifier is enabled only after codec and I²S setup
succeeds. Drag the volume slider from 0% to 100% to change the ES8311 DAC
volume while the firmware is running.

Build and flash with PlatformIO Core:

```sh
pio run -e speaker-diag
pio run -e speaker-diag -t upload
pio device monitor -b 115200
```

Look for `display=ready touch=ready speaker=ready` on the serial monitor. It
also prints the note and frequency each time a key is pressed. If the screen
or codec fails to initialize, the firmware reports that component as
`not_ready`.
