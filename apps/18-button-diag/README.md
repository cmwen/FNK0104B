# FNK0104B button diagnostic

This standalone diagnostic reads the momentary button between expansion GPIO14
and board GND using the ESP32-S3 internal pull-up. It prints debounced
`button=pressed` and `button=released` events over USB CDC serial. Pressed is
active-low; no external resistor is needed.

Build and flash with PlatformIO Core:

```sh
pio run -e button-diag
pio run -e button-diag -t upload
pio device monitor -b 115200
```
