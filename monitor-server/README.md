# FNK0104B local Codex monitor bridge

This Python 3 standard-library server exposes a small HTTP interface for the
FNK0104B monitor and connects to the existing Codex app-server daemon through
its local Unix-socket WebSocket control endpoint. The default daemon can report
currently loaded sessions. An isolated stdio process cannot see the daemon's
loaded session state and is available only as an explicit fallback. The bridge
uses the Codex CLI's existing local sign-in; it does not accept or store an
OpenAI API key. It is a local development service, not a public endpoint.

For controlled board tests without Codex, use the separate [mock server](MOCK.md).

For a complete first-time setup, follow [Connect the monitor to its local server](../docs/monitor-setup.md).
It covers Wi-Fi, host/key configuration, WSL LAN forwarding, authenticated
verification, and the local firmware build/upload. Web BLE configures volume
and idle timeout; it does not currently configure the bridge host/key.

## Run

From the repository root:

To use the same LAN address, port, and key as the device's ignored
`apps/codex-monitor/include/monitor_secrets.h`, run:

```sh
MONITOR_LOG_REQUESTS=1 python3 monitor-server/run.py
```

Environment variables override values from the header. The launcher reads
literal definitions only, never executes the header, and sets new agents' default
workspace to this repository. Optional request logging records client addresses,
endpoint names and HTTP status codes; it omits keys, bodies and query values.
Leave this process running while using the board. It does not install a startup
service. With no local header, it uses the server's loopback default.

To use the existing local speech facade on port 8790, start the bridge with:

```sh
TRANSCRIBE_URL=http://127.0.0.1:8790/v1/audio/transcriptions \
TRANSCRIBE_MODE=multipart MONITOR_LOG_REQUESTS=1 python3 monitor-server/run.py
```

The speech service must be running separately. Its overall health can report
missing speech synthesis or language-specific models even when the default
English transcription model is installed; verify transcription itself before
testing a spoken board command.

For environment-only configuration, run:

```sh
python3 monitor-server/server.py
```

The default listener is `127.0.0.1:8765`. The default app-server socket is
`~/.codex/app-server-control/app-server-control.sock`; override it with
`CODEX_APP_SERVER_SOCKET`. Set `CODEX_APP_SERVER_TRANSPORT=stdio` only to
explicitly use the isolated process. New Codex threads use the bridge process's
working directory; set `MONITOR_AGENT_CWD` to an absolute project path to
choose another workspace. For board access over the LAN, bind to
the host's LAN address and set a local shared key:

```sh
MONITOR_HOST=192.168.1.20 MONITOR_PORT=8765 MONITOR_TOKEN='choose-a-local-secret' python3 monitor-server/server.py
```

For every non-loopback bind, `MONITOR_TOKEN` is required. All `/v1` GET and POST requests
must then include `X-Monitor-Key: <same key>`. Keep that value in ignored
local firmware configuration; never commit it or put it in the device UI.
The bridge does not log request bodies, audio, transcripts, or keys. Set
`TRANSCRIBE_URL` to a trusted transcription endpoint. Default
`TRANSCRIBE_MODE=raw` sends raw WAV with `Content-Type: audio/wav`. For
LocalLink Speech Recognition, set `TRANSCRIBE_MODE=multipart` and point
`TRANSCRIBE_URL` to `http://<host>:<port>/v1/audio/transcriptions`; the bridge
sends multipart/form-data with the WAV in field `file`. Either mode accepts
JSON `{"text":"recognized words"}` or `{"transcript":"recognized words"}`.
The bridge forwards no API credential to the board. WAV upload is limited to
10 MiB. If `TRANSCRIBE_URL` is unset, `/v1/voice` returns HTTP 503
`transcription_unconfigured` and does not create or steer a thread.

The default connection uses the existing Codex daemon's control socket and
never silently starts a second app-server. Set `CODEX_APP_SERVER_TRANSPORT=stdio`
only to explicitly start `codex app-server --listen stdio://`; that separate
process may report threads as `notLoaded` and does not represent the existing
daemon's sessions. Socket, authentication, and request errors are surfaced as
unavailable/degraded integration responses; missing integration is never
reported as connected.

## Manage with LocalLink

The native process can be managed by LocalLink/PM2 using the app-owned
`Dockerfile.locallink` blueprint and `monitor-server/start-locallink.sh`.
The wrapper reads the existing shared key from the ignored firmware header;
there is no need to duplicate the key in the LocalLink workspace.

Set these values in the LocalLink workspace's local `.env`:

```dotenv
FNK_MONITOR_BIND_HOST=0.0.0.0
FNK_MONITOR_PORT=8765
FNK_MONITOR_LOG_REQUESTS=1
FNK_MONITOR_TRANSCRIBE_URL=http://127.0.0.1:8790/v1/audio/transcriptions
FNK_MONITOR_TRANSCRIBE_MODE=multipart
```

Leave the transcription URL empty if voice is not configured. Declare a native
service in `locallink.services.yml` (adjust `cwd` for your checkout):

```yaml
services:
  - name: fnk-codex-monitor
    group: pm2
    runtime: pm2
    runtimeName: fnk-codex-monitor
    cwd: ../FNK0104B
    blueprint: Dockerfile.locallink
    portEnv: FNK_MONITOR_PORT
    envVars:
      - FNK_MONITOR_BIND_HOST
      - FNK_MONITOR_PORT
      - FNK_MONITOR_LOG_REQUESTS
      - FNK_MONITOR_TRANSCRIBE_URL
      - FNK_MONITOR_TRANSCRIBE_MODE
```

Add the entry to the existing `services` list, rather than replacing other
services. In LocalLink, use the **fnk-codex-monitor** card's Start, Stop, Restart
and logs controls. Stop a manually launched bridge before starting this service
so that only one process owns port 8765. Save the workspace PM2 list after adding
the service. PM2 restarts crashed processes; after a computer/WSL restart,
LocalLink still needs to be started so it can restore that saved list. This
configuration does not install an operating-system startup service.

For a manual launch with the same wrapper, export the settings above from the
repository root and run `bash monitor-server/start-locallink.sh`. For a simple
foreground launch without LocalLink, use the commands in **Run** above.

## HTTP contract

### `GET /v1/status`

Success response:

```json
{
  "integration": "connected",
  "codex": {"usage": {
    "five_hour": {"used_percent": 32, "resets_at": 1780000000},
    "weekly": {"used_percent": 60, "resets_at": 1780500000}
  }},
  "agents": [{"id": "thread-id", "name": "Fix display", "status": "running", "detail": "..."}],
  "total_agents": 1,
  "updated_at": 1780000000,
  "errors": []
}
```

`integration` is `connected`, `degraded`, or `unavailable` and describes the
app-server connection. Each usage value is `null` if Codex did not report a
quota bucket with the exact 300-minute (five-hour) or 10,080-minute (weekly)
window. `resets_at` and `updated_at` are Unix epoch seconds. Agent entries are
limited to `running`, `needs_attention`, and `error`; completed/idle threads
are omitted. The bridge returns at most eight entries and reports the count of
all visible active entries in `total_agents`. App-server runtime `systemError` threads map to `error`. The bridge
requests `cli`, `vscode`, `appServer`, `subAgent`, and `subAgentThreadSpawn`
source kinds explicitly. A full app-server failure returns HTTP 503 with `integration:
unavailable` and null usage values. A rate-limit read failure with a working
thread list returns HTTP 200 with `integration: degraded`, available agents,
null usage values, and a short `errors` entry.

The bridge can see only runtime state reported as loaded by its app-server
connection. If the daemon returns persisted `notLoaded` summaries and an empty
loaded-thread list, `agents` is empty and `errors` explicitly says live runtime
states are unavailable; `integration` is `degraded` even when the daemon and
quota API respond, while quota values remain available. The bridge does not
resume old threads to manufacture live status.

### `POST /v1/commands/text`

Send JSON `{"text":"...", "agent_id":"optional-thread-id"}`. `text` must
contain 1–4000 characters. Omit `agent_id` to create a thread and start a
turn in `MONITOR_AGENT_CWD` (or the bridge working directory). With an active `agent_id`, the bridge steers its currently active turn.
With an idle thread id, it resumes that thread and starts a turn. If a single
Codex user-input question is pending, the text answers that question and the
action is `answered_question`. Multi-question prompts must be handled in
Codex. Approval requests show `Approval needed in Codex app` (with a short
reason when available); the bridge never grants approvals, and targeted
text/voice commands return `pending_approval` without steering. The response
is `{ "ok": true, "transcript": "...", "action": "created|steered|answered_question",
"agent_id": "...", "message": "..." }`. Errors use
`{ "ok": false, "error": { "code": "...", "message": "..." } }`.

### `POST /v1/voice?agent_id=<optional-thread-id>`

Send the raw RIFF/WAVE file body with `Content-Type: audio/wav`; maximum body
size is 10 MiB. The optional query parameter routes the recognized text to an
existing thread. The response matches `/v1/commands/text` and includes the
recognized `transcript`. WAV upload requires `TRANSCRIBE_URL`; when absent, the
server responds HTTP 503 with `transcription_unconfigured` and does not issue
any app-server command. A single pending user-input question is answered by
the recognized text. The bridge never accepts pending approvals through voice.

## Protocol and development

The bridge implements the documented Codex app-server sequence over the Unix
WebSocket control socket: initialize once, acknowledge with `initialized`,
answer one-question `item/tool/requestUserInput` server requests when the user
targets that thread, and use `thread/list`,
`account/rateLimits/read`, `thread/start` / `thread/resume`, `turn/start`, and
`turn/steer`. It relies on app-server's active status/flags for attention and
uses the active turn ID as `turn/steer.expectedTurnId`.

The bridge keeps a blocking reader on the existing Codex transport to receive
push events while idle. `thread/*`, `turn/*`, attention requests, and relevant
item events invalidate its status cache. `account/rateLimits/updated` updates
cached quota values without a query. It does not create or resume threads merely
to subscribe to them. Push visibility is limited to what this connection
receives; it does not establish visibility into separately running clients.

Board status requests share a cache. With no visible agents, the bridge refreshes
at most once every 300 seconds unless an event or command invalidates the cache.
With visible agents it checks at most every 15 seconds as a fallback; pushed
changes are pushed to SSE subscribers, coalesced to one check per second. Quota queries are limited to
once per idle-refresh interval unless authentication or transport changes.
Set `MONITOR_IDLE_REFRESH_SECONDS` and `MONITOR_ACTIVE_REFRESH_SECONDS` to
positive finite seconds to change these defaults. Longer idle intervals reduce
work but delay detection of changes for which this connection gets no events.
Connection loss invalidates cached status, and failed requests have a 30-second
retry delay. Concurrent HTTP requests share one refresh.

Refreshes use `thread/list` with `useStateDbOnly: true`, avoiding the default
scan and repair of JSONL history. `updated_at` remains the timestamp of the
agent snapshot; `cache_age_seconds` reports its age, and `quota_updated_at`
reports the last quota query or received quota event. Cached replies do not
claim that every board poll made a fresh Codex query.

`GET /v1/events` is an authenticated SSE subscription using the same
`X-Monitor-Key` header as snapshots. It sends an initial `event: status` JSON
frame, pushes changed values, and sends a comment heartbeat after 60 seconds
without a frame. Timestamp-only changes do not send a status frame. Headers
include `Content-Type: text/event-stream`, `Cache-Control: no-cache`, and
`Connection: close`; no Content-Length or transfer encoding is used.
`GET /v1/events?refresh=1` bypasses caches and retry delays on touch-wake;
`GET /v1/status?refresh=1` retains the same behavior for diagnostic tools.

Fallback refreshes and retries run only while serving a request or SSE
subscriber. Client disconnect cancels that subscription's producer promptly,
including its fallback timer. With no subscribers or snapshot requests, the
bridge keeps only its blocking Codex transport reader; it does not issue
queries or reconnect on a timer. Other Codex clients can still cause daemon
activity independently.

The `codex-monitor` 0.4.0 firmware receives SSE while awake, blocks between
incoming data, and reconnects with 5–60-second backoff after errors. After the
configured inactivity timeout, it closes the stream, turns the backlight off,
and suspends reconnect attempts. Touch wakes it and requests a fresh snapshot.
Wi-Fi association, BLE, and touch handling remain available. An in-flight
connection setup may finish before closing during quiet-mode entry. New-agent
alerts wait for touch while quiet. Powering off the board also closes its
stream; USB disconnection alone does not if another power source remains.
Firmware before 0.4.0 uses snapshot polling instead of SSE.

Run the host-side tests with:

```sh
python3 -m unittest discover -s monitor-server/tests
```

Protocol reference: <https://learn.chatgpt.com/docs/app-server> (JSON-RPC
JSONL/WebSocket transports, initialization, thread/turn methods, and ChatGPT rate limits).
