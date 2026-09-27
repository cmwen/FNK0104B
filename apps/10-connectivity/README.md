# Wi-Fi and BLE diagnostic

The connectivity environment combines the FNK0104B display and touch diagnostics with ESP32-S3 Wi-Fi and Bluetooth LE. Its touchscreen UI uses LVGL 9.6.0, including LVGL's built-in virtual keyboard.

- **Wi-Fi:** scan nearby networks, tap a result or choose **JOIN / EDIT**, enter an SSID and password with the touchscreen keyboard, then tap **JOIN**. The password is masked and is never printed to the serial log. A successful connection saves the Wi-Fi credentials in the ESP32's normal Wi-Fi configuration storage so the board reconnects after reboot.
- **BLE:** scan for nearby BLE advertisements without blocking screen updates, or start advertising as `FNK0104B-DIAG` and find it with a phone BLE scanner.
- **Display updates:** LVGL renders partial regions and the app updates widgets when their state changes, rather than redrawing the full screen on a timer.
- **Serial:** status events are reported at 115200 baud. No Wi-Fi password is logged.

Build and upload with PlatformIO Core:

```bash
pio run -e connectivity
pio run -e connectivity -t upload
pio device monitor -b 115200
```
