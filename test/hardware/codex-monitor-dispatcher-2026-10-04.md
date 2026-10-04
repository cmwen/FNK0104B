# Codex monitor Luna dispatcher — 2026-10-04

The local bridge now implements asynchronous GPT-6 Luna intent/repository
dispatch. The ignored root `.env.dispatcher` configures `/home/cmwen/dev`,
`gpt-6-luna` with `low` effort and `gpt-6.1-sol` for coding tasks. Explicit
environment variables override these local defaults. The bounded startup scan
found 78 repositories, including nested Git worktrees. The existing LocalLink
PM2 service `fnk-codex-monitor` was restarted and left running with this setup.

## Real task checks

The authenticated live text endpoint acknowledged commands in 3–12 ms, then
performed routing asynchronously. A request explicitly naming FNK0104B selected
that repository, started a real worker thread and completed with firmware
version `0.5.0`. A second request, “Fix the bug,” produced “Which repository and
bug should I fix?” without starting a worker. A targeted clarification answer
naming FNK0104B then started and completed a real read-only worker task.

The completed worker thread IDs were:

- `01a10476-2752-7a32-9500-61d357fb14e1` — explicit repository request.
- `01a10476-741e-7b43-a3f8-a93b104da66c` — clarification answer.
- `01a1047b-71c9-7af3-89f5-c3bedd46db60` — final duplicate-request check;
  identical request IDs returned one dispatcher job, and `thread/read` confirmed
  `/home/cmwen/dev/FNK0104B` as the worker's actual workspace. The job returned
  the complete opening sentence with firmware `0.5.0`, and SSE represented it
  as `Done: FNK0104B` with `needs_attention` status.

All 60 bridge/dispatcher tests passed. They cover nested worktrees, dependency
and symlink exclusion, repository boundary changes, asynchronous intake,
idempotency and conflicting request IDs, invalid model output, clarification
context, worker completion, full JSON result collection, background voice
transcription, and preservation of approval blocking. Existing socket/SSE tests
also passed. Router sandbox requests were adapted to the installed daemon after
it rejected the documented `readOnly.access` form; it uses the previously
verified `thread/start` read-only sandbox mode.

## Firmware and serial

PlatformIO Core built `codex-monitor`: 2,998,869 flash bytes and 122,188 static
RAM bytes. The queued acknowledgement now reads “Voice queued; follow dispatcher
avatar,” and voice uploads send an `X-Request-ID`. CLI upload to `/dev/ttyACM0`
completed with all image hashes verified. NVS remains at `0x9000`/`0x5000`, the
factory app at `0x10000`/`0x600000`, and models at `0x610000`/`0x9f0000`.
No full-chip erase, partition-boundary change or security-setting change occurred.

The initial post-upload serial observation showed repeated AFE feed-ring-full
warnings without recognition reports. A normal USB reset through the PlatformIO
serial monitor restored startup, ongoing recognition and network delivery:

```text
firmware=codex-monitor
version=0.5.0
monitor_speech state=ready wake=Hi_ESP wakenet=wn10_hiesp vadnet=vadnet1_medium multinet=mn7_en
monitor_wifi connected=true status=3
monitor_stream state=connected
monitor_status integration=connected agents=1 first_state=running five_hour_used=81 weekly_used=57
monitor_speech state=wake afe_frames=474 max_inference_us=7193 frame_us=32000 heap=7523 psram=4463672
monitor_status integration=connected agents=4 first_state=needs_attention five_hour_used=81 weekly_used=57
monitor_attention_tone unavailable
monitor_speech state=wake afe_frames=2043 max_inference_us=6391 frame_us=32000 heap=7371 psram=4463824
```

These observations establish real dispatcher-status delivery to the uploaded
board and recovery after reset. The initial warning's cause and recurrence,
speaker availability and long-run heap stability remain **UNKNOWN**. No speech
code or hardware mapping was changed in response to that observation.

## Remaining acceptance boundary

Physical voice capture, upload, transcription accuracy and selected-avatar voice
answers on this uploaded build remain **UNKNOWN** until the owner speaks a test
instruction. Host tests verify that voice enters the same asynchronous dispatch
path; they do not establish physical speech accuracy. The separate speech service
still supplies transcription. Models run through the signed-in provider rather
than offline on the bridge host.

Jobs and request IDs are held in memory. Bridge restart/retention eviction loses
them; already-created Codex tasks persist. The router catalog refreshes on bridge
restart. The router has a read-only sandbox and instructions not to use tools;
tool availability itself is not disabled. These limits are documented in the
[bridge guide](../../monitor-server/README.md#luna-repository-dispatcher).
