# LocalLink speech client

This firmware records English speech from the FNK0104B's onboard microphone,
discovers the Speech Recognition service over DNS-SD, and shows the response's
JSON `text` field on the LVGL display. Recording stops after 20 seconds by
default, or earlier when the user taps **STOP**. It sends one WAV file as
multipart field `file`. It does not call `/health`; the aggregate endpoint can
be unhealthy when optional language or synthesis models are absent.

## Hardware and wiring

The supported target is the Freenove FNK0104B 2.8-inch display board. It uses the
onboard MEMS microphone through the ES8311 codec, so no external mic wiring is
needed. Verified defaults are in `lib/fnk0104b/src/fnk0104b/pins.hpp`:

| Signal | GPIO |
|---|---:|
| ES8311 I²C SDA / SCL | 16 / 15 |
| I²S MCLK / BCLK / WS | 4 / 5 / 7 |
| ESP32 I²S data out / data in | 6 / 8 |
| ES8311 I²C address | `0x18` |

The mic capture format is mono, signed 16-bit PCM at 16 kHz, wrapped in a PCM
WAV header. The ES8311 clock uses 6.144 MHz MCLK (384 × 16 kHz). Those audio
settings follow the model-specific Freenove Echo example and Espressif's
ES8311 driver configuration. They have not been checked on this physical board
in this task.

## Configuration

- **Wi-Fi:** copy `include/locallink_secrets.example.h` to
  `include/locallink_secrets.h` and set your SSID and password. That local file
  is ignored by Git. Leave both values empty to reconnect to Wi-Fi credentials
  already saved in the board's NVS, for example by the `connectivity` app.
- **DNS-SD instance:** the default exact instance name is `Speech Recognition`.
  Override `LOCALLINK_SERVICE_INSTANCE` in `include/locallink_config.h` when the
  speech backend advertises a different name. This is separate from LocalLink,
  the app used to manage local services.
- **Recording duration:** `LOCALLINK_RECORD_SECONDS` in
  `include/locallink_config.h` sets the maximum from 1 to 20 seconds (default
  20). During capture, the button changes to **STOP**; tap it to send the audio
  collected so far. The uploaded WAV header reflects the shorter duration.
- **Board/audio pins:** edit `LOCALLINK_PIN_*` in the same config header, or
  override the `FNK0104B_AUDIO_*` defaults in `fnk0104b/pins.hpp` with PlatformIO
  build flags. These are board wiring values and should only be changed for a
  different verified wiring.
- **Fallback endpoint:** set `LOCALLINK_FALLBACK_HOST`, `LOCALLINK_FALLBACK_PORT`,
  and `LOCALLINK_FALLBACK_PATH` in `include/locallink_config.h`, or override
  them in the ignored `include/locallink_secrets.h` for a machine-local endpoint.
  The fallback is disabled by default. DNS-SD remains the normal runtime
  endpoint source. If multicast is unavailable, use a reserved LAN IPv4 address
  for the fallback host; a `.local` name still requires mDNS. The current service
  TXT record advertises the path `/v1/audio/transcriptions`.

The device searches for the exact instance **Speech Recognition** under
`_http._tcp.local`, then uses that record's SRV host and port and its TXT
`path`. It resolves the discovered SRV host over mDNS before connecting by IP;
the HTTP `Host` header still uses the discovered name. The ESP32 does not need
Tailscale; the service and board must share a LAN that carries mDNS multicast.
DNS-SD and HTTP calls run in a worker task. DNS-SD and mDNS host lookup each
wait at most 1.8 seconds, TCP connect at most 5 seconds, and HTTP response reads
use a 9-second timeout. Wi-Fi association is asynchronous and uses
auto-reconnect. Error states can be retried with the **RECORD** button.

## Build and flash

Run from the repository root with PlatformIO Core:

```sh
pio run -e locallink
pio run -e locallink -t upload
pio device monitor -b 115200
```

The environment keeps the project's pinned ESP32 platform, Arduino framework,
flash, partition, USB CDC, and PSRAM configuration. No Arduino IDE is required.

## On-device transcription check

1. Ensure the service advertises **Speech Recognition** on the same
   LAN under `_http._tcp.local`, with a reachable SRV host/port and a TXT `path`.
2. Flash `audio-diag` first and confirm the serial monitor reports
   `microphone_capture=complete ... signal=detected` while you speak.
3. Configure Wi-Fi as above, then flash `locallink`.
4. Open the display and wait for **Wi-Fi connected** and the ready prompt.
5. Tap **RECORD**, say an English phrase, then tap **STOP** when finished (or
   let the 20-second maximum expire). Wait for **Transcription complete**. The
   recognized text appears in the panel; serial output contains only startup
   and device status, never microphone samples, transcripts, or Wi-Fi
   credentials.
6. If it fails, use the displayed status and 115200-baud serial status to check
   Wi-Fi, DNS-SD service visibility, the service TXT path, and HTTP status. The
   app does not treat aggregate `/health` as a transcription gate.

The firmware build and host-side protocol tests do not replace a microphone,
network, or service integration check on the physical board.
