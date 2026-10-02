# Codex monitor mock board run — 2026-09-29

Board: connected FNK0104B; physical PCB revision not independently read. Firmware: `codex-monitor` 0.2.0 working-tree build, landscape revision. Toolchain: pinned `platformio/espressif32@7.0.1`. Port: the board's persistent Espressif USB serial by-id link, 115200 baud. Host mock: `monitor-server/mock_server.py` on the local LAN with an ignored local token. Neither token nor Wi-Fi credentials were logged.

## Build and upload

`pio run -e codex-monitor` passed with 89,796 bytes static RAM (27.4%) and 1,432,205 bytes flash (45.5% of the 3 MiB app slot). `pio run -e codex-monitor -t upload` used esptool over USB; all written hashes verified. This reused the existing `app3M_fat9M_16MB.csv` layout. No erase-all or partition-layout change was requested.

## Observed serial transitions

The board reached the LAN mock from 192.168.1.149. Its HTTP poll counter increased. Selected 115200-baud lines:

```text
monitor_status integration=connected agents=0 five_hour_used=29 weekly_used=51
monitor_status integration=connected agents=1 first_state=running five_hour_used=0 weekly_used=100
monitor_status integration=connected agents=1 first_state=needs_attention five_hour_used=0 weekly_used=100
monitor_attention_tone played
monitor_status integration=connected agents=0 first_state=none five_hour_used=0 weekly_used=100
monitor_status integration=unavailable
monitor_status integration=connected agents=0 first_state=none five_hour_used=29 weekly_used=51
```

Before the landscape/redraw revision, the board also received `error`, `degraded`, and six-agent overflow scenarios. After the revision, the running scenario was held through roughly three five-second polls; only the initial state-change line appeared, consistent with no repeated full-screen redraw. The owner confirmed that the landscape avatar screen looked stable and readable. The mock ended in idle at 29%/51%.

## Animated avatar revision

The owner later reported that the running avatar stayed still. The renderer was receiving a constant animation tick. The revised monitor advances the existing hover, gaze, and activity frames every 100 ms and redraws only the avatar area. The owner confirmed that animation was visible, then observed green flashes from repainting the avatar's state-colored frame before each image transfer. A subsequent build leaves that frame in place during animation and transfers only the avatar pixels. The owner confirmed that the green flashing is gone. This final build passed at 89,804 bytes static RAM (27.4%) and 1,433,021 bytes flash (45.6%); its USB upload hashes verified. The 115200-baud monitor recorded `monitor_status integration=connected agents=1 first_state=running five_hour_used=29 weekly_used=51` after upload.

The black-background revision also enlarged the idle quota bars and replaced the small network outlines with solid Wi-Fi and Codex status icons. The owner confirmed that the larger icons are clear. Wi-Fi auto-reconnect is enabled after an earlier loss of LAN reachability, but a controlled disconnect/reconnect test has not been run.

## Limits

Serial and the mock's HTTP counter establish that the board polled and parsed the states. They do not prove individual pixels, touch hit targets, audible speaker output, actual WAV upload from the board, BLE read/write behavior, or live Codex app-server agent visibility. The tone line means the firmware audio path initialized and issued playback; speaker presence is not electrically detectable.
