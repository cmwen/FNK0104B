# Live Codex monitor check, 2026-10-02

## Setup

- FNK0104B USB device: `/dev/ttyACM0`, VID:PID `303a:1001`, serial
  `B8:1F:3F:C3:9F:94`.
- PlatformIO serial monitor attached at 115200 baud. Its capture contained the
  monitor header but no new firmware lines during unchanged status polls.
- Real bridge: `192.168.1.32:8765`, using the ignored device configuration's key.
- Local transcription: `http://127.0.0.1:8790/v1/audio/transcriptions`, multipart.
- No firmware upload, flash erase, partition change, or security setting change.

## Verified

The bridge repeatedly received authenticated requests from board address
`192.168.1.159` and returned live Codex status:

```json
{"event":"monitor_request","client":"192.168.1.159","endpoint":"/v1/status","http_status":200,"integration":"connected"}
```

A host request to the same authenticated bridge's `/v1/commands/text` asked
Codex to reply with `monitor test successful` without tools or file changes.
The bridge returned HTTP 200, `ok: true`, and `action: created` for thread
`01a0fbe4-c834-7e52-9228-546cdbcb3ab3`. A subsequent read confirmed turn status
`completed`, response `monitor test successful.`, and no turn error. The first
immediate read encountered an empty rollout metadata file; a subsequent read
succeeded. This verifies the live text command path, not microphone input.

## Board voice result

After the owner performed the spoken test, the bridge received `/v1/voice`
from `192.168.1.159` and returned HTTP 200. The voice request created thread
`01a0fbe9-867d-7fc1-9cd8-45d1a53e046f`. Its stored user input was
`to test successful. Do not change any`. The turn completed without error,
replying `What would you like me to test? I’ll leave all files unchanged.`

This establishes the board microphone-to-upload-to-local-transcription-to-Codex
path. The recognized instruction was incomplete, so the intended fixed-response
test did not pass. Whether the loss occurred in recording timing, audio quality,
or transcription is not established. Targeted voice replies remain unverified.

## Updated firmware upload

The owner requested direct flashing after the MCP dashboard denied access.
`pio run -e codex-monitor` passed, then
`pio run -e codex-monitor -t upload --upload-port /dev/ttyACM0` succeeded.
The uploaded app was 1,434,560 bytes including image overhead, with hash
verification. This firmware includes background status polling, header usage,
voice-stage feedback and the const-input ArduinoJson lifetime fix.
The existing partition layout was retained; no erase-all or security setting
change was performed. The board resumed successful live bridge polling.

Post-upload serial monitoring at 115200 baud and a subsequent standard USB
reset/capture produced no new firmware lines. The upload is verified, but
numeric display values and physical touch responsiveness await visual checks.

## Still pending

## Timeout diagnosis and verified quotas

The owner saw a red Codex icon and unknown usage after the first direct upload.
On-screen and serial diagnostics identified `Status HTTP -11` for every poll.
A host timing probe measured a real status response at 3.07 seconds, longer
than the firmware's 2.5-second read timeout. Status reads now allow 15 seconds
and connections allow 5 seconds; polling remains in its own task.

The revised direct CLI upload completed with hash verification (1,434,896-byte
app image). Serial monitoring at 115200 baud then captured:

```text
monitor_status integration=connected agents=1 first_state=running five_hour_used=24 weekly_used=88
```

This verifies the new board firmware receives and parses both live quota
values. A physical check of the revised voice-button interaction remains open.

Physical display appearance, touch selection, BLE settings, speaker audibility,
sleep/wake and actual Wi-Fi reconnection were not established by HTTP polling.
