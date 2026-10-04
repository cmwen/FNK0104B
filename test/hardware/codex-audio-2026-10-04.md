# USB microphone and Codex Micro validation — 2026-10-04

Board: existing Freenove FNK0104B ESP32-S3, 16 MB flash and 8 MB PSRAM.
PlatformIO Core CLI remains the build, upload and serial-monitor interface.
The final image is installed and USB is available to Windows/Desktop.

## Findings and fixes

### “Mic unavailable” and the restart loop

The old UI used local speech-recognition readiness as microphone readiness.
It could report “Mic unavailable” for model/task failures without identifying a
physical microphone problem. Raw I2S capture and local recognition are now
separate, with specific error reasons and independent USB microphone output.

An isolated audio diagnostic ran for several minutes with raw microphone ready.
The combined image subsequently panicked during WakeNet creation. Saved RTC
records showed `reset_reason=4`, `previous_failure size=84 caps=0x804` and speech
startup stage 3. The model partition loaded at stage 2; the image had only 21,311
internal heap bytes before WakeNet creation. This establishes internal allocation
failure in that combined image. The precise original pre-audio failure is UNKNOWN.

The recovery image keeps DMA and task stacks internal, moves CPU-only HID JSON
state, reply storage and speech FIFO into PSRAM, reduces the unused USB speaker
OUT FIFO to 128 bytes, and budgets radio buffers/internal code placement.
`CONFIG_FREERTOS_PLACE_FUNCTIONS_INTO_FLASH=y` moves selected non-ISR functions;
ISR placement stays under IDF's existing rules. The final running monitor had
9,983 free internal heap bytes with local speech and the bridge connected.

### Windows microphone Code 10

Linux enumerated mono PCM16/16 kHz audio, but Windows initially created two
independent MEDIA devices, MI_03 and MI_04. Both reported Code 10, status
`0xc0000182`, with `wdma_usb.inf`/`usbaudio`. Arduino's pinned UAC1 microphone
template omitted an IAD while CDC already supplied one.

[Microsoft's composite enumeration documentation](https://learn.microsoft.com/en-us/windows-hardware/drivers/usbcon/support-for-interface-collections)
explains that existing IADs disable legacy audio interface grouping. A board-level
compatibility header now prepends an eight-byte audio IAD and adjusts the total
microphone descriptor length. Installed framework sources remain unchanged.
After flashing, Windows reported one healthy MEDIA `TinyUSB UAC1` device and
AudioEndpoint `Microphone (TinyUSB UAC1)`, alongside healthy HID, composite and CDC.

### Build and upload integration

The forced descriptor header must include SDK configuration before checking the
audio flag. PlatformIO also needs explicit dependencies for the USB descriptor
objects because its scanner does not track compiler `-include` headers. Both
issues are corrected; preprocessing the actual generated USBAudioCard command
confirmed the active IAD and extra eight descriptor bytes.

TinyUSB uses the same pinned commit, downloaded as a checksum-verified archive
instead of a full repository clone. IDF environments share managed components;
build them sequentially to avoid replacing a dependency during another link.

A default RTS reset retained ROM download mode in recovery. The configured
esptool watchdog reset started the application. A PlatformIO 1200-baud request
successfully entered recovery without physical buttons. USB/IP occasionally lost
its connection during model upload; detach/reattach cleared a stale transport
and the final full upload then verified every image hash.

No GPIO, partition boundary, NVS erase, eFuse or bootloader security setting changed.

Validation used the existing working tree, including separate, pre-existing
dispatcher changes. Those dispatcher changes are outside this firmware commit.

## Final image and physical evidence

- Application uploaded: 3,102,096 bytes; static RAM: 151,604 bytes.
- Firmware SHA256: `05873cff38c46c449b6af2c866f76e9b5265a7875c90cb60316d0aa8ac5cd8a1`.
- USB identity: `303a:8360`, CDC COM9 in this Windows session.
- Post-flash health: `wifi=3 status_task=1 voice_task=1 speech=1 speech_reason=none`.
- Raw microphone: `ready=1 reason=none`; VAD reacted to sound during recovery.
- WakeNet `wn10_hiesp`, VADNet `vadnet1_medium` and MultiNet `mn7_en` initialized.
- Bridge SSE and agent status connected; the captured LCD shows Wi-Fi Online,
  Micro status and both voice controls without an unavailable-microphone label.
- Final image completed 5,338 consecutive AFE frames (over 170 seconds at 32 ms)
  without reset. This is a short recovery soak, not sustained-use validation.
- A live Windows-side `device.status` request decoded and its reply completed.
  Protocol activity alone does not authenticate which host application sent it.
- Windows microphone/audio, HID and serial device statuses all reported OK.

See [final serial extract](codex-audio-final-2026-10-04.log),
[recovery serial extract](codex-audio-recovered-2026-10-04.log) and
[actual recovery LCD](codex-audio-recovered-2026-10-04.png).
The LCD was captured before the final descriptor-only fix.

Windows audio ownership prevented normal WSL reattachment after the final flash.
Required post-flash monitoring used PlatformIO at 115200 through a temporary
localhost-only Windows COM9 bridge. It closed after 60 seconds; no permanent
service or system setting changed, and USB remains available to Windows.

## Verification and remaining checks

- Monitor, isolated audio diagnostic and HID diagnostic builds passed.
- Native protocol/framing tests passed: 24 tests, including ACT10 key encoding.
- Existing host screenshot tests passed: 5; existing bridge tests passed: 60.
- Upload hashes, Windows driver status and post-flash serial health verified.
- Whitespace check passed.

Still UNKNOWN: captured host PCM quality, sustained USB recording, physical
ACT10 touch delivery, configured Desktop Voice Chat/transcription, Wi-Fi voice
submission under USB recording, and Wi-Fi-outage/USB-reconnect independence.
Select **Microphone (TinyUSB UAC1)** in Windows/Desktop and test the board's
**Micro voice** control. The existing **Wi-Fi voice** path remains separate.

## Official sources

- [ESP-IDF 5.5.5 RAM usage](https://docs.espressif.com/projects/esp-idf/en/v5.5.5/esp32s3/api-guides/performance/ram-usage.html)
  and [FreeRTOS flash placement](https://docs.espressif.com/projects/esp-idf/en/v5.5.5/esp32s3/api-reference/kconfig-reference.html#config-freertos-place-functions-into-flash).
- [Microsoft interface grouping](https://learn.microsoft.com/en-us/windows-hardware/drivers/usbcon/support-for-interface-collections)
  and [IAD guidance](https://learn.microsoft.com/en-us/windows-hardware/drivers/usbcon/usb-interface-association-descriptor).
- [Arduino ESP-IDF component integration](https://docs.espressif.com/projects/arduino-esp32/en/latest/esp-idf_component.html)
  and [pinned official USB component source](https://github.com/espressif/esp32-arduino-lib-builder/tree/6671d0bd65cdb9d4cc1001b759e8610de945a8d5/components/arduino_tinyusb).
- [Official Codex Micro user guide](https://learn.chatgpt.com/docs/features/codex-micro):
  host microphone selection and Mic/Voice Chat mappings. This is user-flow guidance,
  not documentation of this firmware's unofficial wire protocol.

See [reference index](../../knowledge/references.md#usb-microphone-recovery-references-2026-10-04)
and [Desktop setup](../../docs/codex-audio.md).
