# Wi-Fi provisioning diagnostic

The Wi-Fi environment starts Espressif SoftAP provisioning and displays its QR code on the FNK0104B screen. Use Espressif's **ESP SoftAP Provisioning** phone app to scan it and send the board the credentials for a 2.4 GHz Wi-Fi network.

```bash
pio run -e wifi
pio run -e wifi -t upload
pio device monitor -b 115200
```

The QR contains a proof-of-possession value generated for the current provisioning session. Wi-Fi credentials are not compiled into the source. The provisioning manager stores accepted credentials in NVS; with `reset_provisioned` set to `false`, later boots reconnect using the saved credentials. Serial output reports provisioning status and the assigned IP address without printing the network password.
