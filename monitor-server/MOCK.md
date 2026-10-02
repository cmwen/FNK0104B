# Monitor mock server

The mock implements the board-facing endpoints in [the monitor contract](../docs/monitor-contract.md) without a Codex app-server or speech service. It lets a physical board receive controlled idle, agent, quota, and connection states. No mock command starts or replies to a Codex agent.

Firmware 0.4.0 subscribes to `/v1/events`: an initial SSE status frame is
followed by changed state and a 60-second comment heartbeat. Scenario, agent
and quota controls push updates on that connection. Offline state is sent as
`integration: unavailable` within an HTTP 200 stream. Client disconnection
stops that stream. Older firmware can continue using `/v1/status` snapshots;
both endpoints accept `refresh=1` and require the same monitor key.

## Start

For host-only testing:

```sh
MONITOR_PORT=8765 python3 monitor-server/mock_server.py
```

For a board on the same trusted LAN, bind the host's LAN address and use a local key. Replace `192.168.1.20` with your host's address. The example keeps the key out of shell history and source control:

```sh
python3 -c 'import pathlib,secrets; p=pathlib.Path("/tmp/fnk0104b-monitor-mock-token"); p.write_text(secrets.token_hex(16)+"\n"); p.chmod(0o600)'
MONITOR_HOST=192.168.1.20 MONITOR_PORT=8765 MONITOR_TOKEN_FILE=/tmp/fnk0104b-monitor-mock-token python3 monitor-server/mock_server.py
```

Set the same host, port, and key in the ignored `apps/codex-monitor/include/monitor_secrets.h` (copy its example first). Build `pio run -e codex-monitor` before uploading. The mock requires `X-Monitor-Key` on both board and control endpoints when bound to a non-loopback address.

## Change states

From the host, use `mock_control.py`. In a second terminal, set the URL and token file once (again replacing the sample address):

```sh
export MONITOR_MOCK_URL=http://192.168.1.20:8765
export MONITOR_TOKEN_FILE=/tmp/fnk0104b-monitor-mock-token
python3 monitor-server/mock_control.py scenario idle
python3 monitor-server/mock_control.py scenario running
python3 monitor-server/mock_control.py scenario attention
python3 monitor-server/mock_control.py scenario error
python3 monitor-server/mock_control.py scenario complete
python3 monitor-server/mock_control.py scenario offline
```

The other scenarios are `degraded` and `overflow`. `offline` responds to `GET /v1/status` with HTTP 503; `degraded` returns HTTP 200 with `integration: degraded`. The mock starts at 29% five-hour and 51% weekly usage. Set percentages with `usage 0 100`; null usage can be sent directly to `POST /_mock/usage`.

For a custom agent, use:

```sh
python3 monitor-server/mock_control.py agent demo needs_attention --name "Build agent" --detail "Which layout should I use?"
python3 monitor-server/mock_control.py agent demo complete
```

`state` shows the current payload, snapshot count (including SSE status frames) and client IP, and metadata for the last voice or text request. The mock accepts `POST /v1/voice` only to check WAV transport; it returns `action: mock_received` with an empty transcript and does not transcribe. This does not change the real bridge's unconfigured voice behavior.

Run host tests with `python3 -m unittest discover -s monitor-server/tests`.
