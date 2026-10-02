# Codex monitor contract (v1)

The FNK0104B polls a local bridge over HTTP on a trusted LAN. The bridge is the only component that speaks the [Codex app-server protocol](https://learn.chatgpt.com/docs/app-server). The board uses Wi-Fi credentials previously saved through the separate provisioning firmware. Its app-server identity, account access, and speech transcription stay on the host.

## Status

`GET /v1/status` returns HTTP 200 with this shape:

```json
{
  "integration": "connected",
  "codex": {"usage": {
    "five_hour": {"used_percent": 32, "resets_at": 1780000000},
    "weekly": {"used_percent": 60, "resets_at": 1780500000}
  }},
  "agents": [
    {"id": "thread-id", "name": "Fix display", "status": "needs_attention", "detail": "Which layout should I use?"}
  ],
  "total_agents": 1,
  "updated_at": 1780000000,
  "errors": []
}
```

`integration` is `connected`, `degraded`, or `unavailable` for the app-server connection. This is distinct from the board's Wi-Fi state. Usage percentages are integers from 0 to 100. A missing exact 300-minute or 10,080-minute Codex quota window is reported with `used_percent: null` and `resets_at: null`; the board displays an unknown value rather than inventing one. Times are Unix seconds.

`agents` contains up to eight `running`, `needs_attention`, and `error` entries. `total_agents` counts all visible active entries in the current app-server page, including those omitted by this response cap. Completion removes an avatar on the next successful poll. `id` is the Codex thread id used for a targeted reply. The board uses a 320×240 landscape layout: the idle page shows quotas, while an active agent page shows a larger avatar and retains remaining percentages in the header. Up to four agents appear with an overflow count. The screen redraws when visible state changes. `detail` is brief display text and can be empty. The board treats unsuccessful or stale polls as loss of bridge connectivity and never reports an old quota as freshly connected. The attached app-server daemon currently returns pre-existing threads as `notLoaded`, including this active conversation. Those records are omitted; seeing agents started in another Codex process requires a shared live runtime or another integration path.

## Voice commands

The board records at most a short mono 16-bit, 16 kHz WAV segment after the user taps **Voice command**. It shows microphone preparation before recording, then **Stop recording** when ready to listen. Tapping again stops early. It sends raw WAV bytes with `Content-Type: audio/wav` to `POST /v1/voice`. With no selected agent, no target is supplied and the bridge creates a Codex thread. When the user opens a `needs_attention` avatar, the request uses `?agent_id=<URL-encoded thread id>` so the bridge routes recognized speech to that thread. Successful submission briefly displays the recognized text. Status polling runs in a separate task from touch and drawing; remaining quota stays visible in the header while avatars occupy the main panel. The UI shows `100 - used_percent` and labels it `left`; the HTTP contract continues to carry used percentages, with null values shown as unknown.

The bridge accepts WAV bodies up to 10 MiB. It returns JSON with `ok`, `transcript`, `action`, `agent_id`, and `message`, or an `error` object. The host must have `TRANSCRIBE_URL` configured for voice; otherwise it returns `transcription_unconfigured` without starting work. `POST /v1/commands/text` with `{"text":"...","agent_id":"optional-thread-id"}` is a development fallback. The firmware does not print WAV data or transcripts over serial.

App-server approval requests are distinct from a question the agent asks. The bridge must not turn a spoken phrase into command, file, or permission approval without an explicit review flow. Until such a flow exists, the board can alert and show that approval is needed in a Codex client, while simple user-input questions can receive a targeted voice answer when the bridge receives that pending question.

## BLE settings

The monitor advertises `FNK0104B-MONITOR` with service UUID `4e4b0104-0001-4d20-8f4b-0104b0000001` and one read/write settings characteristic `4e4b0104-0002-4d20-8f4b-0104b0000001`. Its four bytes are version `1`, volume percent `0..100`, and a little-endian idle display timeout in minutes `1..120`. Defaults are 50% volume and 30 minutes. The setting is stored in NVS. The display backlight turns off only after that much continuous idle time. Any visible active agent (`running`, `needs_attention`, or `error`), including overflow agents, or voice preparation/recording/submission keeps it on and resets the idle countdown. Touch resets the countdown; touch or new agent activity wakes the display. Web BLE labels the stored setting **Idle screen timeout**; the verified board support provides on/off control, not a measured dimming level. The speaker circuit has no verified plugged-in detection; a notification tone is attempted when the audio path initializes. The separate Wi-Fi BLE provisioning service is unchanged.

## Local development boundary

The bridge listens on `127.0.0.1:8765` by default. To let the board connect, explicitly bind a LAN address and set `MONITOR_TOKEN`; all `/v1` calls then need the matching `X-Monitor-Key` header. Store the matching value in the ignored `apps/codex-monitor/include/monitor_secrets.h`. New threads use the bridge's working directory unless `MONITOR_AGENT_CWD` chooses another project. This HTTP mode is for a trusted local network. Do not expose the bridge or app-server transport to the public internet. See [the bridge README](../monitor-server/README.md) for run commands and the current app-server attachment method.

For board tests without Codex, [the mock server](../monitor-server/MOCK.md) implements the same `/v1/status` and voice transport surface with separate state controls.
