# FNK0104B microphone diagnostic

This standalone PlatformIO target initializes the onboard ES8311 microphone
path and repeatedly captures one second of mono, 16-bit, 16 kHz I²S input. It
reports only the captured sample count and whether a signal was detected; it
does not print or save microphone samples.

The target uses the verified defaults in `lib/fnk0104b/src/fnk0104b/pins.hpp`:
MCLK/BCLK/WS on GPIO 4/5/7, ESP32 data out/in on GPIO 8/6, ES8311 over I²C on
SDA/SCL 16/15 at address `0x18`. No external microphone wiring is needed.

Build and flash with PlatformIO Core:

```sh
pio run -e audio-diag
pio run -e audio-diag -t upload
pio device monitor -b 115200
```

Speak near the onboard microphone and look for
`microphone_capture=complete ... signal=detected`. A `quiet` result can mean the
board is silent or the codec/mic path needs inspection. Run this diagnostic on
the physical board before relying on the combined `locallink` firmware.
