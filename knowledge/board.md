# Board facts

- **Board:** Freenove FNK0104B, 2.8-inch capacitive-touch ESP32-S3 Display;
  240 × 320 IPS panel. The generic `esp32-s3-devkitc-1` PlatformIO profile is a
  software build target, not the physical board. [F1, F2, F3]
- **MCU:** ESP32-S3R8 (schematic marking), dual-core Xtensa LX7, up to 240 MHz;
  8 MB internal OPI PSRAM and 16 MB external SPI flash. A WROOM module designation,
  fitted flash manufacturer, physical PCB revision, and component markings are
  **UNKNOWN**. [F2, F3]
- **Display:** ILI9341 driver family (ILI9341V in module specification), SPI;
  CS 10, DC 46, SCK 12, MOSI 11, MISO 13, active-high backlight 45. Reset shares
  CHIP_PU; there is no separate display-reset GPIO. [F1, F2, F3, R1]
- **Touch:** FT6336 family, I²C address `0x38`, SDA 16, SCL 15, active-low reset 18
  and interrupt 17. Exact fitted suffix is **UNKNOWN**: specification says FT6336G,
  Freenove examples use FT6336U. [F2, F3, F4]
- **Audio:** ES8311 at I²C `0x18`, shared SDA/SCL 16/15; I²S MCLK 4, BCLK 5,
  WS 7, ESP32 input 6, ESP32 output 8. MEMS microphone and external speaker
  connector; amplifier enable 1 is active low. Fitted amplifier part is **UNKNOWN**.
  GPIO6/8 directions follow Freenove Echo code and the repository's recorded
  microphone test; the specification uses codec-relative input/output wording.
  [F2, F3, F5, R1, R2]
- **MicroSD:** four-bit SDIO/SD_MMC, CLK 38, CMD 40, DAT0 39, DAT1 41, DAT2 48,
  DAT3 47. Freenove overview prose says SPI, but the schematic and model-specific
  example agree on SDIO. [F2, F3, F6, R1]
- **Other wiring:** addressable RGB LED 42; BOOT button 0; RESET to CHIP_PU;
  battery ADC 9; USB D− 19 / D+ 20; UART0 TX 43 / RX 44. Expansion GPIO connector
  exposes 2, 3, 14, 21; separate expansion I²C shares 15/16. Connector numbering
  and voltage must be checked against the schematic before attachment. [F2, F3]

## Allocation restrictions

GPIO0, 3, 45, 46 are strapping pins. Do not apply new loads/pulls that change reset
levels. GPIO0 low with GPIO46 low selects download mode; GPIO45 selects VDD_SPI
voltage, and GPIO3 affects the JTAG source. Preserve board reset wiring and
existing pulls; actual device eFuse overrides are **UNKNOWN**. GPIO26–37 serve
flash/OPI PSRAM and must not be allocated. GPIO22–25 do not exist on ESP32-S3.
GPIO19/20 must stay available for onboard USB. GPIO43/44 are on UART0; reclaiming
them requires deliberately relinquishing UART functionality. [E1, E2, E3, F2]

GPIO2 and GPIO21 are the best **conditional candidates**: exposed, no assignment
found in current firmware, and not strapping or memory pins. GPIO14 is already
used as an active-low pull-up button input by `button-diag` and `locallink`; it is
available only if that button and those firmware uses are absent. GPIO3 is exposed
but requires a reviewed strapping design. No other onboard peripheral pins are
classified as freely available. Actual attached expansion devices, acceptable
load/current, and external pull resistors are **UNKNOWN**. [F2, F3, R1, E1]

## USB/UART and existing firmware

Type-C connects directly to native ESP32-S3 USB, not an assumed CH340/CP210x bridge.
The existing build enables USB CDC on boot and monitors at 115200 baud. A previous
session recorded Espressif `303a:1001` on `/dev/ttyACM0`; port names and current
attachment must be discovered each time. UART0 is also exposed on its four-pin
connector. Firmware uses display/touch, SD, audio, Wi-Fi/BLE/OTA, and GPIO14
in different named environments; these are not all active in every app. RGB and
battery ADC pins are declared but have no initialization/read found in current apps. [F2, R1, R2]

Existing display setup selects `INVON` based on the 2026-09-28 physical color test.
Microphone direction was tested on 2026-09-29. Preserve these recorded results.
The original factory partition table is **UNKNOWN**. Read `platformio.ini` for
current per-environment partition/memory settings; do not infer them from generic
board metadata. [R2]
