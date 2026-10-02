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
