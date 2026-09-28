# Wi-Fi setup over BLE

This standalone firmware advertises `FNK0104B-SETUP` over Bluetooth LE when
the board has no saved Wi-Fi credentials. It uses Espressif's provisioning
protocol with Security 1 and a fresh 12-character proof-of-possession code
shown on the FNK0104B display. The GitHub Pages flasher uses Web Bluetooth to
send a 2.4 GHz Wi-Fi name and password through that encrypted session. The
provisioning manager saves successful credentials in the board's normal Wi-Fi
NVS configuration; the later `locallink` speech firmware can reuse them.

1. In the [web flasher](../../docs/flashing.md), choose **Wi-Fi over BLE** and
   install it over USB, or use `pio run -e wifi-ble` followed by
   `pio run -e wifi-ble -t upload`.
2. Read the code on the board. In the flasher's **Set up Wi-Fi** section,
   enter that code and select `FNK0104B-SETUP` in the Bluetooth picker.
3. Enter the 2.4 GHz network name and password. Wait for **Wi-Fi connected**
   on the board, then install `locallink`.

Use Chrome or Edge in a secure context with Bluetooth enabled. The website
does not save the password. The code changes on every boot, so enter the code
currently shown on the board. Serial status is available at 115200 baud and
does not print Wi-Fi credentials. A normal firmware upload does not erase the
NVS partition.

If credentials are already saved, this firmware reconnects to that network and
does not advertise provisioning. Use the `connectivity` firmware's touchscreen
Wi-Fi screen to change an existing network without clearing NVS.
