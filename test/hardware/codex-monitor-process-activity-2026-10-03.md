# Bridge-triggered Codex daemon activity, 2026-10-03

The observations below precede the firmware change. The subsequent
[SSE upload and idle observation](codex-monitor-sse-2026-10-03.md) replaced
board polling with a persistent stream and added automatic quiet mode.

Host-only observation on the running WSL bridge; no firmware upload or hardware
configuration change. The board at `192.168.1.159` was polling over Wi-Fi.

## Measurement

Sampled Linux `/proc/<pid>/stat`, `/proc/<pid>/io`, and thread activity every
five seconds for 60 seconds. CPU percentages are CPU seconds divided by elapsed
wall-clock seconds, relative to one logical CPU. Reads below are `rchar` (bytes
returned by read operations, including cached data), not physical disk traffic.
The active T3 conversation used a separate app-server process.

| Condition | Duration | Daemon CPU | Read bytes |
| --- | ---: | ---: | ---: |
| Original bridge, board polling | 60 s | 31.07% | 760,004,860 |
| Bridge paused with SIGSTOP; Codex socket retained | 30 s | 0.20% | 1,583,638 |
| Original bridge resumed | 30 s | 38.50% | 479,158,467 |
| Fixed bridge after restart, board polling | 60 s | 5.97% | 343,246,727 |

The original Python bridge itself averaged 0.05% CPU. Its daemon was PID 1407725
and the bridge was PID 1289754. The fixed bridge was restarted through its
existing PM2 `fnk-codex-monitor` service and became PID 1417039. The pause script
restored the original bridge using SIGCONT in a `finally` block.

## Cause and change

Every board status request called `thread/list`, `thread/loaded/list`, and
`account/rateLimits/read`. Daemon logs confirmed repeated calls from the bridge
connection. `thread/list` omitted `useStateDbOnly`, so the default history scan
and metadata repair ran on every poll. The
[official protocol documentation](https://learn.chatgpt.com/docs/app-server#list-threads-with-pagination--filters)
documents `useStateDbOnly: true` as returning database results without scanning
JSONL thread logs to repair metadata.

The bridge now sends that option. A live database-only request returned 50
thread summaries, and all 20 host tests passed. During the fixed minute the
board received ten logged HTTP 200 status responses. Excluding the first ten
seconds after restart, daemon CPU averaged 3.48% over the remaining 50 seconds.
Ongoing status queries still cause some daemon work.

## Limits and disconnect behavior

This test establishes that the bridge requests caused most of the observed
daemon CPU load; it does not prove that every daemon wakeup comes from the
bridge. No syscall trace was collected. The daemon and other clients can have
independent background activity.

The bridge has no background status polling loop. Powering off the board or
stopping its Wi-Fi requests stops this source of queries. USB disconnection
alone does not do so if the board has another power source. The pause test kept
the Codex connection open, indicating that the connection alone did not sustain
the measured CPU load. A physical board disconnect was not performed.

Status responses were `degraded` both before and after the change because this
daemon reported persisted threads as `notLoaded`; there were no live agents
visible on this connection. HTTP success verifies continued board-to-bridge
traffic, not visibility into other Codex clients. No command or voice turn was
started during these checks.

## Follow-up: push events and idle cache

The bridge was then changed to receive Codex WebSocket events continuously with
a blocking reader. Relevant thread and turn events invalidate its status
snapshot; quota events update cached usage without a query. Incoming board
requests share the cache, with a 300-second idle fallback, a 15-second active
fallback, and a 30-second retry delay after failures. Quotas have their own
observation timestamp and expire after the idle-refresh interval. There is no
timer that issues queries or reconnects while the board is absent. Connection
loss invalidates the cache. Commands invalidate it immediately.

All 36 host tests passed. The added tests exercise unsolicited events without
client requests, idle ping/pong, disconnect detection, request timeouts,
transport-generation isolation, concurrent HTTP callers, cache expiry, event
invalidation, account changes, quota updates, retry recovery, and timestamps.

The final code was restarted through the existing PM2 service. After initial
status queries populated the cache, `/proc` activity was sampled every five
seconds for another 60 seconds:

| Process | PID | Average CPU | Read bytes (`rchar`) |
| --- | ---: | ---: | ---: |
| Codex daemon | 1407725 | 0.033% | 827,492 |
| Bridge | 1420621 | 0.050% | 0 |

The board received twelve HTTP 200 responses in that minute. An additional
authenticated host probe returned `cache_age_seconds: 60` and retained the
original `updated_at`, confirming that cached observations were not relabeled
as new queries. Daemon request logs contained **zero requests** between the
measurement start and final host probe. Request timestamps, including their
nanosecond fractions, were used to exclude startup records that were committed
to the log database after the warm-up queries completed.

The daemon still reported the same persisted-only visibility limitation; this
test does not establish push delivery for threads in other Codex processes.
Push handling was tested with local synthetic transport events. No new Codex
turn, firmware upload, physical disconnect, sleep schedule, or board Wi-Fi
power-management change was performed. CPU measurements demonstrate reduced
host work; board current draw and total energy were not measured.

The board firmware continues to poll HTTP every five seconds even with its
backlight off. A separate firmware change is needed to reduce radio activity
or implement touch-wake, scheduled, or manual away/sleep behavior. Events
received by the bridge trigger fresh status on the next board poll; changes
without an event may take up to the idle fallback interval to be discovered.
