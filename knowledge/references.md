# Evidence and authoritative references

Source IDs used in `board.md` and `pins.yaml`:

- **F1:** [Freenove model comparison](https://docs.freenove.com/projects/fnk0104/en/latest/fnk0104/codes/MAIN/Freenove_ESP32S3_Display.html): FNK0104B size, resolution, driver family.
- **F2:** [Official 2.8-inch schematic](https://github.com/Freenove/Freenove_ESP32_S3_Display/blob/main/Schematic/2.8inch_ESP32-S3_Display_Schematic.pdf): electrical nets, ESP32-S3R8 marking, connectors. Re-fetched 2026-10-02.
- **F3:** [Freenove-packaged module specification](https://github.com/Freenove/Freenove_ESP32_S3_Display/blob/main/Datasheet/ES3C28P_ES2N28P_Specification_V1.0.pdf): internal title ES3C28P & ES3N28P, V1.0, 2025-06-14; memory/controller facts and §4.1–4.2 connector/GPIO allocation. Re-fetched 2026-10-02. Vendor text contains inconsistencies; cross-check chip capabilities with Espressif.
- **F4:** [Freenove touch tutorial](https://docs.freenove.com/projects/fnk0104/en/latest/fnk0104/codes/MAIN/11_TFT_Touch.html) and [packaged FT6336G datasheet](https://github.com/Freenove/Freenove_ESP32_S3_Display/blob/main/Datasheet/D-FT6336G-DataSheet-V1.0.pdf). The fitted G/U suffix remains UNKNOWN.
- **F5:** [Freenove Echo source](https://github.com/Freenove/Freenove_ESP32_S3_Display/tree/main/Tutorial_With_Touch/Sketches/Sketch_07.2_Echo): I²S direction and ES8311 configuration. [Espressif ES8311 driver](https://github.com/espressif/esp-adf-libs/tree/master/esp_codec_dev/device/es8311) provides codec implementation references.
- **F6:** [Freenove SD tutorial](https://docs.freenove.com/projects/fnk0104/en/latest/fnk0104/codes/MAIN/6_SD_Card.html): use the FNK0104B SD_MMC example, not its conflicting SPI overview prose.
- **E1:** [ESP32-S3 GPIO documentation](https://docs.espressif.com/projects/esp-idf/en/v5.0/esp32s3/api-reference/peripherals/gpio.html): GPIO ranges, straps, USB, OPI memory reservations.
- **E2:** [ESP32-S3 series datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf) and [technical reference manual](https://www.espressif.com/sites/default/files/documentation/esp32-s3_technical_reference_manual_en.pdf): chip electrical limits and peripheral registers.
- **E3:** [Espressif boot-mode selection](https://docs.espressif.com/projects/esptool/en/latest/esp32s3/advanced-topics/boot-mode-selection.html) and [hardware schematic checklist](https://docs.espressif.com/projects/esp-hardware-design-guidelines/en/latest/esp32s3/schematic-checklist.html): strap/reset restrictions.
- **R1:** Current repository [pin definitions](../lib/fnk0104b/src/fnk0104b/pins.hpp), [TFT setup](../lib/fnk0104b/src/tft_setup.h), [board implementation](../lib/fnk0104b/src/board.cpp), audio implementations in the same directory, [button diagnostic](../apps/18-button-diag/src/main.cpp), [LocalLink app](../apps/12-locallink/src/main.cpp), and [PlatformIO environments](../platformio.ini). Inspected 2026-10-02; source inspection proves configuration/use, not physical device behavior.
- **R2:** Existing [hardware evidence](../docs/hardware.md), [pin documentation](../docs/pins.md), and [monitor readiness](../docs/monitor-readiness.md): prior device tests, unresolved markings and open checks. These observations were not rerun during this setup.

Tooling references:

- [Codex MCP configuration](https://developers.openai.com/codex/mcp): trusted-project `.codex/config.toml` and OAuth login.
- [Official Espressif Documentation MCP](https://developer.espressif.com/blog/2026/04/doc-mcp-server/): `https://mcp.espressif.com/docs`, authenticated retrieval of public technical documentation.
- [Official ESP-IDF Tools MCP](https://developer.espressif.com/blog/2026/04/esp-idf-tools-mcp-server/): ESP-IDF 6.0+ prerequisite; not configured for this PlatformIO/Arduino project.
- [Third-party PlatformIO MCP source](https://github.com/jl-codes/platformio-mcp), [npm release](https://www.npmjs.com/package/platformio-mcp/v/3.1.0), and [release validation](https://github.com/jl-codes/platformio-mcp/blob/main/docs/CODEX_PLUGIN_RELEASE.md): existing-CLI adapter, Node >=20, pinned runtime, operator policy.
- [PlatformIO Core CLI](https://docs.platformio.org/en/latest/core/): canonical build/upload/device-monitor workflow.

For additional Freenove tutorials and existing evidence see [docs/references.md](../docs/references.md).

- **E4:** [Espressif built-in USB-JTAG setup](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-guides/jtag-debugging/configure-builtin-jtag.html): native GPIO19/20 USB-JTAG wiring and Linux USB permissions; chip capability, not a completed board debug session.
- **E5:** [ESP-SR source and component requirements](https://github.com/espressif/esp-sr), [ESP32-S3 benchmarks](https://docs.espressif.com/projects/esp-sr/en/latest/esp32s3/benchmark/README.html): WakeNet10/command recognition support. Speech library requires IDF >=5; Arduino 2.0.17 is based on IDF 4.4.7.

- **E6:** [Espressif ILI9341 component 2.1.0](https://components.espressif.com/components/espressif/esp_lcd_ili9341/versions/2.1.0/readme): ESP-IDF SPI panel driver, initialization and native LCD API; used by the visual speech diagnostic.

- **E7:** [Espressif VADNet](https://docs.espressif.com/projects/esp-sr/en/latest/esp32s3/vadnet/README.html) and [AFE](https://docs.espressif.com/projects/esp-sr/en/latest/esp32s3/audio_front_end/README.html): neural VAD model selection, activity states, minimum speech/silence durations and cached pre-speech audio. Used by speech diagnostic 0.3.0.

- **E8:** [Espressif audio codec 2.5.0](https://components.espressif.com/components/espressif/esp_audio_codec/versions/2.5.0/readme): Opus encoder/decoder APIs and supported settings, used by the recorder. [Ogg Opus mapping (RFC 7845)](https://www.rfc-editor.org/rfc/rfc7845) specifies container headers, granule positions and trimming. Host container verification and short device recording/playback checks are recorded in the recorder README; audible output and sustained operation remain UNKNOWN.

- **E9:** [Espressif USB device stack (ESP-IDF 6.0)](https://docs.espressif.com/projects/esp-idf/en/v6.0/esp32s3/api-reference/peripherals/usb_device.html): TinyUSB HID keyboard/mouse and composite device support; USB-OTG and USB Serial/JTAG share one PHY. Consulted 2026-10-04 for the documentation guide. This establishes chip capability, not a tested FNK0104B HID implementation. [Android USB host overview](https://developer.android.com/develop/connectivity/usb/host) explains that phone host capability is device dependent.
- **E10:** [Espressif Arduino USB API](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/usb.html) and [Arduino 2.0.17 USB HID keyboard implementation](https://github.com/espressif/arduino-esp32/blob/2.0.17/libraries/USB/src/USBHIDKeyboard.h): USB device initialization, raw keyboard usages, HID LED events, and composite support. Cross-checked against the installed pinned framework sources on 2026-10-04; runtime support is tracked separately in the HID hardware checklist.

## USB microphone recovery references (2026-10-04)

- **E11:** [ESP-IDF 5.5.5 RAM usage](https://docs.espressif.com/projects/esp-idf/en/v5.5.5/esp32s3/api-guides/performance/ram-usage.html) and [FreeRTOS flash placement](https://docs.espressif.com/projects/esp-idf/en/v5.5.5/esp32s3/api-reference/kconfig-reference.html#config-freertos-place-functions-into-flash): internal RAM/IRAM budgeting and moving selected non-ISR task functions to flash. DMA and task stacks stay internal; CPU-only HID state and speech FIFO use PSRAM.
- **E12:** [Arduino as an ESP-IDF component](https://docs.espressif.com/projects/arduino-esp32/en/latest/esp-idf_component.html) and [official USB component source](https://github.com/espressif/esp32-arduino-lib-builder/tree/6671d0bd65cdb9d4cc1001b759e8610de945a8d5/components/arduino_tinyusb): the integration follows the pinned installed Arduino 3.3.12/IDF 5.5.5 sources; latest documentation may describe newer versions.
- **M1:** [Microsoft composite interface grouping](https://learn.microsoft.com/en-us/windows-hardware/drivers/usbcon/support-for-interface-collections) and [IAD guidance](https://learn.microsoft.com/en-us/windows-hardware/drivers/usbcon/usb-interface-association-descriptor): when IADs are present, Windows uses them instead of legacy audio grouping. Adding an audio IAD resolved two Code 10 interfaces on this board.
- **O1:** [Official Codex Micro guide](https://learn.chatgpt.com/docs/features/codex-micro): selected host microphone, Mic key mappings and Voice Chat behavior. This describes the supported retail product's user flow, not an official wire protocol or endorsement of this firmware's compatibility identity.

Device observations and unresolved acceptance checks are in [the audio validation record](../test/hardware/codex-audio-2026-10-04.md).
