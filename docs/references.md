# References

Hardware facts were checked on 2026-09-27 against the Freenove-maintained online guide and the resources in Freenove's official `Freenove_ESP32_S3_Display` repository. The official repository states that its source and circuit files use CC BY-NC-SA 3.0; this project links to those materials and does not copy their code or assets.

## Official Freenove sources

- [Freenove FNK0104 online documentation](https://docs.freenove.com/projects/fnk0104/en/latest/) — product tutorials and resources.
- [Model comparison](https://docs.freenove.com/projects/fnk0104/en/latest/fnk0104/codes/MAIN/Freenove_ESP32S3_Display.html) — FNK0104B panel size, resolution, and ILI9341 driver.
- [Preface and hardware overview](https://docs.freenove.com/projects/fnk0104/en/latest/fnk0104/codes/MAIN/Preface.html) — Type-C USB, onboard components, SD overview, and serial setup.
- [Serial tutorial](https://docs.freenove.com/projects/fnk0104/en/latest/fnk0104/codes/MAIN/1_Serial.html) — USB CDC and 115200 baud serial behavior.
- [Touch tutorial](https://docs.freenove.com/projects/fnk0104/en/latest/fnk0104/codes/MAIN/11_TFT_Touch.html) — FT6336U driver use and I²C/reset/interrupt GPIO assignments.
- [SD and music example](https://docs.freenove.com/projects/fnk0104/en/latest/fnk0104/codes/MAIN/7_Music.html) — FNK0104B SD_MMC and audio example pin definitions.
- [Freenove board-menu profile image](https://docs.freenove.com/projects/fnk0104/en/latest/_images/Chapter07_09.png) — 16 MB flash, QIO 80 MHz, USB CDC on boot, OPI PSRAM, and “16M Flash (3MB APP/9.9MB FATFS)” partition selection.
- [RGB tutorial](https://docs.freenove.com/projects/fnk0104/en/latest/fnk0104/codes/MAIN/2_RGB.html) — one WS2812 LED on GPIO42 for the 2.8-inch model.
- [Official Freenove example repository](https://github.com/Freenove/Freenove_ESP32_S3_Display) — source examples, packaged display/touch libraries, schematics, and datasheets.
- [2.8-inch display schematic (PDF)](https://github.com/Freenove/Freenove_ESP32_S3_Display/blob/main/Schematic/2.8inch_ESP32-S3_Display_Schematic.pdf) — board electrical connections, including GPIO and peripheral nets.
- [ES3C28P/ES2N28P module specification (PDF)](https://github.com/Freenove/Freenove_ESP32_S3_Display/blob/main/Datasheet/ES3C28P_ES2N28P_Specification_V1.0.pdf) — memory sizes, component descriptions, connector details, and GPIO allocation.
- [FT6336 datasheet (PDF)](https://github.com/Freenove/Freenove_ESP32_S3_Display/blob/main/Datasheet/D-FT6336G-DataSheet-V1.0.pdf) — controller-family reference.
- [ESP32-S3 datasheet (PDF)](https://github.com/Freenove/Freenove_ESP32_S3_Display/blob/main/Datasheet/esp32-s3_datasheet_en.pdf) — chip-level reference.

## Tooling references

- [PlatformIO Core CLI](https://docs.platformio.org/en/latest/core/) and [recommended Linux installer](https://docs.platformio.org/en/latest/core/installation/methods/installer-script.html) — user-level CLI installation.
- [PlatformIO Espressif32 platform](https://docs.platformio.org/en/latest/platforms/espressif32.html) — ESP32-S3 build options and PSRAM flags.
- [PlatformIO ESP32-S3 DevKitC-1 board definition](https://docs.platformio.org/en/latest/boards/espressif32/esp32-s3-devkitc-1.html) — generic target used as the supported PlatformIO software board profile.
- [PlatformIO native unit testing](https://docs.platformio.org/en/latest/advanced/unit-testing/index.html) — host-side test framework and workflow.
