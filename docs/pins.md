# Verified FNK0104B pin map

GPIO values are centralized in `lib/fnk0104b/src/fnk0104b/pins.hpp`. These values are for FNK0104B only; other FNK0104 display sizes use different mappings.

| Peripheral | Signal | ESP32-S3 GPIO | Notes | Source |
|---|---|---:|---|---|
| Display | CS | 10 | LCD chip select | 2.8-inch schematic; module spec §4.2 |
| Display | DC/RS | 46 | Data/command select | 2.8-inch schematic; module spec §4.2 |
| Display | SCK | 12 | SPI clock | 2.8-inch schematic; module spec §4.2 |
| Display | MOSI | 11 | LCD write data | 2.8-inch schematic; module spec §4.2 |
| Display | MISO | 13 | LCD data return | 2.8-inch schematic; module spec §4.2 |
| Display | Reset | CHIP_PU | Shared ESP32-S3/LCD reset; no separate GPIO | 2.8-inch schematic; module spec §4.2 |
| Display | Backlight | 45 | Active high per module spec | 2.8-inch schematic; module spec §4.2 |
| Touch | SDA | 16 | Shared I²C bus | Schematic; Freenove touch example |
| Touch | SCL | 15 | Shared I²C bus | Schematic; Freenove touch example |
| Touch | Reset | 18 | Active low per module spec | Schematic; module spec §4.2; touch example |
| Touch | Interrupt | 17 | Active low per module spec | Schematic; module spec §4.2; touch example |
| Touch | I²C address | `0x38` | Controller suffix is unresolved; see `hardware.md` | Schematic; module spec |
| MicroSD | CLK | 38 | SDIO clock | Schematic; module spec §4.2; Freenove SD_MMC example |
| MicroSD | CMD | 40 | SDIO command | Schematic; module spec §4.2; Freenove SD_MMC example |
| MicroSD | DAT0 | 39 | Four-bit SDIO data | Schematic; module spec §4.2; Freenove SD_MMC example |
| MicroSD | DAT1 | 41 | Four-bit SDIO data | Schematic; module spec §4.2; Freenove SD_MMC example |
| MicroSD | DAT2 | 48 | Four-bit SDIO data | Schematic; module spec §4.2; Freenove SD_MMC example |
| MicroSD | DAT3 | 47 | Four-bit SDIO data | Schematic; module spec §4.2; Freenove SD_MMC example |
| RGB LED | WS2812 data | 42 | One addressable pixel | Schematic; Freenove RGB example |
| Audio | Amplifier enable | 1 | Active low per module spec | Schematic; module spec §4.2 |
| Audio | I²S MCLK | 4 | ES8311 codec bus | Schematic; module spec §4.2; Freenove Echo example |
| Audio | I²S BCLK | 5 | ES8311 codec bus | Schematic; module spec §4.2; Freenove Echo example |
| Audio | I²S DOUT | 6 | ESP32-S3 output to codec | Schematic; module spec §4.2; Freenove Echo example |
| Audio | I²S WS/LRCK | 7 | ES8311 codec bus | Schematic; module spec §4.2; Freenove Echo example |
| Audio | I²S DIN | 8 | Codec-to-ESP32-S3 input | Schematic; module spec §4.2; Freenove Echo example |
| Audio | I²C SDA/SCL | 16 / 15 | Shared with touch | Schematic; module spec §4.2; Freenove Echo example |
| Audio | ES8311 I²C address | `0x18` | CE pin low | Freenove ES8311 example driver |
| Audio/touch/expansion | I²C SDA/SCL | 16 / 15 | Shared with touch and extension I²C | Schematic; module spec §4.2 |
| System | BOOT button | 0 | Also download-mode select | Schematic; module spec §4.2 |
| System | Battery voltage ADC | 9 | Analog battery monitor input | Schematic; module spec §4.2 |
| USB | D− / D+ | 19 / 20 | Internal USB bus | Schematic; module spec §4.2 |
| UART0 | TX / RX | 43 / 44 | Also available on 4-pin serial connector | Schematic; module spec §4.2 |
| Expansion header | GPIOs | 2, 3, 14, 21 | 4-pin expansion connector | Module specification §4.1–4.2 |

Several peripherals share signals. In particular, GPIO15/16 are shared by touch, audio codec control, and expansion I²C. Do not initialize one consumer in a way that disrupts another. The SD card is wired for 4-bit SDIO despite the overview page's SPI wording; do not substitute the unrelated SPI SD pin map from other Freenove boards.
