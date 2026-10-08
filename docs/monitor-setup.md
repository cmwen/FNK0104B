# Connect the Codex monitor to its local server

The board connects over Wi-Fi to a Python bridge on your computer. The bridge
reads the locally signed-in Codex app-server and optionally sends recorded audio
to a separate speech service. GitHub Pages configures BLE settings; it does not
host the bridge or carry board status/audio.

## 1. Prepare the computer and network

Use Python 3 and this repository on Linux, macOS, or in WSL. The bridge's default
transport requires the existing Codex app-server Unix socket in that same
environment: `~/.codex/app-server-control/app-server-control.sock`. Keep the
signed-in Codex session running. If its socket is elsewhere, set
`CODEX_APP_SERVER_SOCKET` to that path. A Windows-native Codex session does not
automatically provide this socket inside WSL.

Connect the board to 2.4 GHz Wi-Fi on a network that can reach the computer.
Use **Device setup → Set up Wi-Fi** after installing **Wi-Fi over BLE** firmware
if credentials have not been saved. That firmware shows a setup code and
advertises `FNK0104B-SETUP`. If credentials already exist, use the `connectivity`
firmware's Wi-Fi screen to change networks. The monitor reuses saved credentials;
a normal upload preserves NVS.

Find the computer's LAN IPv4 address in its network settings (`ipconfig` on
Windows). Reserve it in your router if possible. The examples below use
`192.168.1.20`; replace it with the address your board can reach. Do not use
`localhost`, `127.0.0.1`, `0.0.0.0`, or a WSL NAT address as the board's host.

## 2. Configure the board's server address and shared key

From the repository root:

```sh
cp apps/codex-monitor/include/monitor_secrets.example.h apps/codex-monitor/include/monitor_secrets.h
python3 -c 'import secrets; print(secrets.token_hex(32))'
```

Copy the generated key into the ignored header. Example values:

```cpp
#pragma once
#define MONITOR_SERVER_HOST "192.168.1.20"
#define MONITOR_SERVER_PORT 8765
#define MONITOR_SERVER_TOKEN "paste-your-generated-key-here"
```

Do not commit this header. These values are compiled into the monitor firmware;
Web BLE currently changes only alert volume and idle screen timeout. A public
GitHub Pages monitor build has no private bridge host/key. Use your locally
configured build for the real bridge. Changing host, port or key requires a new
build/upload.

## 3. Start the bridge

In a terminal at the repository root:

```sh
MONITOR_HOST=0.0.0.0 MONITOR_LOG_REQUESTS=1 python3 monitor-server/run.py
```

The launcher reads the port/key from the ignored header; `MONITOR_HOST` overrides
its bind address so the listener accepts LAN traffic. Expect
`FNK monitor bridge listening on http://0.0.0.0:8765`. Keep this terminal running;
Ctrl+C stops the bridge. This command does not install an automatic startup
service. Permit TCP 8765 through the host firewall on the trusted local network.
The shared key is required for a LAN listener. Do not expose this local HTTP
service through router port forwarding to the internet.

To enable voice commands, first start a compatible transcription service in
the same environment, then use:

```sh
MONITOR_HOST=0.0.0.0 MONITOR_LOG_REQUESTS=1 \
TRANSCRIBE_URL=http://127.0.0.1:8790/v1/audio/transcriptions \
TRANSCRIBE_MODE=multipart python3 monitor-server/run.py
```

Port 8790 is the local speech facade used in the recorded board test; the bridge
does not install or start it. Substitute your service's URL. Without speech
configuration, status/usage still work and voice returns
`transcription_unconfigured`. See [bridge options](../monitor-server/README.md).

For a managed process, follow [the LocalLink service setup](../monitor-server/README.md#manage-with-locallink).
It uses the app-owned startup wrapper and blueprint, keeps the key in the same
ignored header, and exposes Start/Stop/Restart in the LocalLink dashboard. Save
the PM2 workspace list and start LocalLink after a Windows/WSL restart to restore
it; merely restarting the board does not require restarting the server.

### WSL 2 network access

In default NAT mode, Windows localhost access does not establish LAN access for
the board. Run the bridge in WSL with the bind command above. In **Administrator
PowerShell**, replace the example LAN address and distribution name:

```powershell
$boardBridgeLanAddress = "192.168.1.20"
$bridgeDistro = "Ubuntu"
$wslBridgeAddress = (wsl.exe -d $bridgeDistro hostname -I).Trim().Split(' ')[0]
netsh interface portproxy add v4tov4 listenaddress=$boardBridgeLanAddress listenport=8765 connectaddress=$wslBridgeAddress connectport=8765
New-NetFirewallRule -Name "FNK0104B-Bridge" -DisplayName "FNK0104B monitor bridge" -Direction Inbound -Action Allow -Protocol TCP -LocalAddress $boardBridgeLanAddress -LocalPort 8765 -RemoteAddress LocalSubnet -Profile Private
```

The board header still uses the **Windows LAN address**. Check forwarding with
`netsh interface portproxy show all`; update it if WSL's address changes after a
restart. Keep the firewall rule scoped to your private LAN. With mirrored
networking, LAN access may work directly, but Windows/Hyper-V firewall rules
still apply. See [Microsoft's WSL networking guide](https://learn.microsoft.com/en-us/windows/wsl/networking).

## 4. Verify the server before flashing

From a second terminal in the repository root, probe using the configured key
without pasting it into a command or the browser:

```sh
python3 - <<'PY'
import importlib.util
from pathlib import Path
from urllib.request import Request, urlopen

spec = importlib.util.spec_from_file_location("monitor_run", "monitor-server/run.py")
import sys
sys.path.insert(0, "monitor-server")
launcher = importlib.util.module_from_spec(spec)
spec.loader.exec_module(launcher)
config = {}
launcher.configure(config, Path("apps/codex-monitor/include/monitor_secrets.h"))
url = f"http://{config['MONITOR_HOST']}:{config['MONITOR_PORT']}/v1/status"
request = Request(url, headers={"X-Monitor-Key": config["MONITOR_TOKEN"]})
with urlopen(request, timeout=30) as response:
    print(response.status)
    print(response.read().decode())
PY
```

Expect HTTP 200, `integration: connected`, and quota objects under `codex.usage`.
An empty agent list can be normal when idle. This probe checks the configured
address from the computer; it does not prove board LAN reachability. On WSL,
also test the Windows LAN address from another LAN device. Opening the status
URL without its header returns 401 by design.

## 5. Build, upload and verify the board

```sh
pio run -e codex-monitor
pio device list
pio run -e codex-monitor -t upload --upload-port /dev/ttyACM0
pio device monitor --port /dev/ttyACM0 -b 115200
```

Replace the serial port with the one reported by `pio device list`; WSL's ACM
number can change after reattachment. Build before upload and close an existing
serial monitor before uploading. The normal upload retains the configured
partition layout and saved Wi-Fi settings; do not use erase-all.

Look for `monitor_wifi connected=true` and
`monitor_status integration=connected ... five_hour_used=... weekly_used=...`.
The display shows Wi-Fi Online, Codex Ready/Busy, and remaining percentages
(`100 - used_percent`). The bridge request log should show the board's requests
to `/v1/status` returning 200. Then open [Device setup](https://cmwen.github.io/FNK0104B/index.html#setup)
to connect to `FNK0104B-MONITOR` and save volume, idle timeout and USB Micro layout over BLE. See [wireless setup and controls](usb-micro-controls.md).

## Troubleshooting

| Symptom | Check |
|---|---|
| Wi-Fi Offline | Saved credentials, 2.4 GHz network, signal and board power. |
| HTTP 401 | The firmware key must match the bridge key; rebuild after a key change. |
| Connection refused/timeout | Bridge terminal, LAN IPv4/port, host firewall, WSL forwarding and router client isolation. |
| `Status HTTP -11` | HTTP read timeout: check bridge/app-server responsiveness and network access. Status polling already allows 15 seconds. |
| HTTP 503 / Codex Off | The bridge may be reachable but unable to query its Codex socket. Read its error response and check the same-environment signed-in app-server. |
| Codex Check / unknown quotas | Inspect `errors` in `/v1/status`; missing quota buckets or unavailable loaded-thread state are reported rather than invented. |
| Voice unavailable | Start the speech service and set `TRANSCRIBE_URL`/`TRANSCRIBE_MODE` before starting the bridge. |

BLE discovery and settings are independent of the Wi-Fi connection to the
bridge. A successful BLE connection does not prove server connectivity.
