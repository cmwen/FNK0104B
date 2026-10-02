# SSE monitor upload and idle observation, 2026-10-03

The owner attached the FNK0104B and authorized flashing. PlatformIO Core CLI
discovered Espressif USB 303a:1001 at `/dev/ttyACM0`.

## Build and upload

- `python3 -m unittest discover -s monitor-server/tests`: 48 tests passed.
- `pio test -e native`: 15 tests passed, including fragmented SSE framing,
  comments, multiline data, unknown events, bounds and incomplete events.
- `pio run -e codex-monitor`: passed; RAM 89,844 bytes, flash 1,445,161 bytes.
- `pio run -e codex-monitor -t upload --upload-port /dev/ttyACM0`: passed,
  hashes verified. Firmware version 0.4.0. Existing partition layout retained;
  no erase-all or security-setting changes. Existing NVS settings retained.
- `pio device monitor --port /dev/ttyACM0 -b 115200`: serial inspected.

The live PM2 `fnk-codex-monitor` service was restarted with SSE support. An
authenticated host probe received HTTP 200, `Content-Type: text/event-stream`,
and an initial `event: status` snapshot. No key or transcript was printed.

## Device connection and recovery

The host observed one persistent TCP connection from board `192.168.1.159`.
Restarting the bridge closed it; serial recorded a retry and recovery:

```text
monitor_stream state=disconnected reason=link
monitor_status integration=unavailable
monitor_stream state=retry wait_ms=5000
monitor_stream state=connected
monitor_status integration=degraded agents=0 first_state=none five_hour_used=26 weekly_used=4
```

A later service restart also recovered. Quotas are observations at the time of
the test. The integration remains degraded because the daemon returns
persisted summaries without live states from other Codex processes.

## Full idle minute

After reconnect and initial snapshots, sampled `/proc/<pid>/stat` and
`/proc/<pid>/io` for 60 seconds. CPU percentages are relative to one logical
CPU; read bytes are `rchar`, not physical disk reads. The daemon's SQLite
request logs were counted by timestamp plus nanoseconds and process identity.

| Process | PID | CPU average | Read bytes |
| --- | ---: | ---: | ---: |
| Codex daemon | 1407725 | 0.033% | 4,196 |
| Bridge | 1428043 | 0.017% | 0 |

There were **zero Codex requests** during the minute. The bridge log contained
only one 13-byte `: heartbeat` frame to the board. There were no new snapshot
GETs, status frames, reconnects, or serial errors. The connection stayed open.
This replaces the previous twelve board snapshot requests per minute.

## Quiet-mode implementation and remaining checks

The existing idle timeout closes the SSE socket using `shutdown()` to wake its
blocking `select()`. Only the status worker closes its descriptor; a mutex
guards it against UI shutdown and descriptor reuse. The worker blocks on a
FreeRTOS notification until touch-wake. Touch clears old displayed status,
requests `/v1/events?refresh=1` and consumes that gesture without starting
voice capture. Wi-Fi association, BLE advertising and touch scanning remain
available. This is application quiet mode, not ESP32 deep sleep.

Host tests verify that client EOF promptly cancels the SSE producer and its
fallback timer; idle heartbeats issue no Codex query; notifications push
changed status; subscribers share caches; fallback refreshes, forced wake
refreshes and unavailable-state recovery work. Native tests cover the parser
and existing idle-timer rules.

## Subsequent physical quiet/touch check

The owner agreed to perform the short check using the existing Web BLE
settings. Serial monitoring at 115200 recorded:

```text
monitor_sleep state=quiet stream=closed
monitor_stream state=disconnected reason=quiet
monitor_status integration=unavailable
monitor_sleep state=awake reason=touch
monitor_stream state=connected
monitor_status integration=degraded agents=0 first_state=none five_hour_used=30 weekly_used=5
```

Host logs independently recorded `closed`, `opened`, and a new 414-byte
`status` frame for the board. The TCP connection used a new client port
(51627 rather than 51626). This verifies physical quiet-mode entry, socket
closure, touch-wake and fresh status delivery. No voice-capture diagnostic
was observed from the wake touch. Exact elapsed timeout and the BLE setting
value were not independently measured. The owner can restore the preferred
timeout through Web BLE.

Active/voice keep-awake behavior with SSE, board current draw and total energy
remain **UNKNOWN**. Changes in other Codex processes may still wait for the
fallback interval because cross-process push visibility has not been
established.
