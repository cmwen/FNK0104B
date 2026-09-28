# Screen timeout diagnostic

Build and upload with `pio run -e screen-timeout -t upload`. The firmware keeps the display backlight on while the touchscreen is active, switches it off after 60 seconds without a touch, and turns it back on when a touch is detected. It polls touch while the backlight is off. The backlight is switched fully off; PWM dimming is not included because the verified board documentation only establishes active-high on/off control.

Use `pio device monitor -b 115200` to inspect touch and screen state messages. Verify the display and touch diagnostics independently before testing this combined firmware on the board.
