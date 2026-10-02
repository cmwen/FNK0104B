# Codex monitor firmware

The FNK0104B shows Codex five-hour and weekly remaining quota on an idle landscape page. The angular cyan status strip shows Wi-Fi signal strength, Codex state, and segmented remaining-quota bars (`5H` and `WK`). When an agent runs, the two quota cards give way to a larger avatar view; remaining percentages stay visible in the header, and up to four agents appear at once. Tap an attentive avatar to read its short detail and direct the next voice command to that thread. A voice command without a selected agent starts a new Codex thread through the local bridge. Completed agents disappear on the next successful status poll. HTTP polling runs separately from touch and drawing so a bridge request does not pause the voice button. The screen redraws only when visible state changes.

![Host-rendered idle screen using the firmware drawing helpers](preview.png)

This preview uses sample values. The cyan/mint icons are tintable masks derived
from the owner's UI references; the firmware renders them in RGB565 at 320×240.

After tapping **Voice command**, wait for **Stop recording** and the listening
message before speaking. The button shows **Preparing mic** during codec
setup and **Sending voice** while transcription runs. Successful submission briefly
shows `Sent:` followed by the recognized text. Serial diagnostics report voice
stages, recording duration, and HTTP result without printing the transcript.
Status requests allow up to 15 seconds for the bridge's Codex queries; this
wait occurs on the polling task and does not pause touch handling. Failed polls
show an HTTP/body/JSON diagnostic below the header and clear stale usage.

The board uses saved Wi-Fi credentials and polls `monitor-server` on the local network. It never connects directly to a Codex account or stores account credentials. The bridge attaches to the local Codex app-server and sends audio to a host transcription service. See [the contract](../../docs/monitor-contract.md) and [bridge setup](../../monitor-server/README.md).
The separate [mock server](../../monitor-server/MOCK.md) provides controllable states for a board test without Codex.

## Configure and build

Follow [the full server-to-board setup guide](../../docs/monitor-setup.md) for
server launch commands, WSL networking and connection checks. The public Pages
firmware has no private bridge host/key; use the configured local build below.

1. Copy `include/monitor_secrets.example.h` to `include/monitor_secrets.h` and set `MONITOR_SERVER_HOST` to the LAN address of the machine running the bridge. Set `MONITOR_SERVER_TOKEN` to the same value as the bridge's `MONITOR_TOKEN`; the copied file is Git-ignored. The board must already have Wi-Fi credentials saved, for example by the `wifi-ble` firmware.
2. Start the bridge with a LAN bind and configure a trusted `TRANSCRIBE_URL` for speech. Without transcription, status still works and the voice request returns a clear error.
3. Run `pio run -e codex-monitor`. Build before upload; then use `pio run -e codex-monitor -t upload` and inspect `pio device monitor -b 115200`.

The project retains the pinned ESP32 platform and existing partition layout. No account secret or Wi-Fi password belongs in this firmware's source code.

## Settings and audio

The monitor advertises `FNK0104B-MONITOR` over BLE automatically at boot; no pairing button or Windows Settings pairing is required. Use the web page's **Connect to monitor** picker. The full name is in the primary advertisement and the settings service UUID is in the scan response, keeping each packet within the legacy BLE size limit. Serial `monitor_ble` messages report packet configuration, advertising start, connection and disconnection. The repository web flasher's **Monitor settings** section reads and saves alert volume and screen timeout. Defaults are 50% and 30 minutes. The Web BLE **Idle screen timeout** setting accepts 1–120 minutes and persists across reboots. Active agents (including agents waiting for input or reporting an error) and voice work keep the screen on. The countdown starts after work finishes and resets on touch. Touch or new agent activity wakes the screen automatically. An audible attention alert requires a speaker on the board's PH1.25 connector. The onboard microphone records voice commands; microphone and speaker share I²S and run sequentially.

App-server approval requests are indicated for review in the Codex app. The monitor does not grant permissions or approvals by voice. Simple agent questions can be answered by a targeted voice command when the bridge has received the pending question.

The integrated firmware was uploaded to a physical board and polled the mock over Wi-Fi on 2026-09-29. The owner confirmed that the landscape avatar screen looked stable and readable. Live Codex thread visibility, on-board voice upload, BLE settings, and audible speaker output remain to be checked; see [readiness](../../docs/monitor-readiness.md).

Quota cards and bars show what remains: `100 - used_percent`. Unknown values stay `--%`; the bridge contract and serial diagnostics continue to report used percentages.
