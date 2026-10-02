# FNK0104B hardware facts

Facts below are from Freenove's model-specific documentation, its official 2.8-inch schematic, and the module specification linked in `references.md`. The electrical design source is the schematic; the product guide and example sketches confirm intended software behavior.

## Verified

| Area | Verified fact |
|---|---|
| Board | FNK0104B, 2.8-inch touch variant of the Freenove ESP32-S3 Display. |
| Main control | ESP32-S3R8 is the device marking in the official schematic; module documentation describes a dual-core Xtensa LX7 ESP32-S3, up to 240 MHz. A separate WROOM module name is not shown for this design. |
| Flash | 16 MB external SPI flash, per the module specification. The exact flash vendor and package marking are not listed. |
| PSRAM | 8 MB OPI PSRAM, per the module specification. |
| Display | 2.8-inch IPS TFT, 240×320, ILI9341. |
| Touch | Capacitive, on I²C address `0x38`. The module specification calls the controller FT6336G; Freenove example code uses its FT6336U driver. Record the suffix difference as unresolved until the fitted part marking is checked. |
| SD card | MicroSD slot wired for 4-bit SDIO. The FNK0104B example uses `SD_MMC`; its six signals are in `pins.md`. The online preface calls the connector SPI, which conflicts with the schematic, module specification, and model-specific example. Use SDIO/SD_MMC as the verified wiring. |
| USB | Type-C connector connects to the ESP32-S3 internal USB bus and supports programming and USB serial. Use USB CDC on boot for `Serial` over this connection. It enumerated as Espressif `303a:1001` at `/dev/ttyACM0` and was successfully used to flash this project. |
| RGB indicator | One WS2812-family addressable RGB LED, data on GPIO42. |
| Audio | ES8311 codec, MEMS microphone, onboard speaker amplifier, and PH1.25 speaker connector. A speaker must be connected there to hear playback. Freenove's model-specific Echo example configures the microphone as 16 kHz mono, 16-bit I²S. The amplifier part number is unresolved: the schematic and included vendor datasheet list do not agree. |
| Power and buttons | USB power; optional 3.7–4.2 V battery connector with charging circuit; BOOT and RESET buttons. |
| Other connections | 4-pin UART0 connector and an expansion header. Their verified signal pins are listed in `pins.md`. |

## Unresolved hardware and device checks

- Touch controller suffix/marking: FT6336G in the module specification versus FT6336U in the Freenove code and packaged driver name.
- Exact fitted audio amplifier part number.
- Exact factory partition table. PlatformIO uses the 16 MB / 3 MB app / 9.9 MB FATFS profile shown in Freenove's tutorial and has already uploaded that table to the connected board, but the original factory table was not saved before the upload.
- Esptool identified the connected chip as ESP32-S3 QFN56 revision v0.2 and reported embedded 8 MB PSRAM. A partial firmware startup capture reported `psram_bytes=8386295` and `free_heap_bytes=371116`, followed by repeated `status=running`. The banner prefix and complete flash-size line were lost when USB detached during reset, so they are not recorded as runtime-verified values.
- The `cmwen` login is in `dialout`; use a fresh login session if serial access again fails with permission denied.
- The physical PCB revision and fitted component markings have not been independently inspected. The WSL ACM number can vary with enumeration state.

These unresolved markings and the original factory table do not block a display/Wi-Fi monitor using the existing board support. The missing device evidence for the combined monitor path is tracked in [monitor readiness](monitor-readiness.md).

Do not treat a value as device-verified until the board is connected and the relevant diagnostic has run. See `pins.md` and `references.md` for source detail.

## Device-verified display behavior

On the connected FNK0104B, the avatar diagnostic's red, green, and blue test
chips appeared as cyan, magenta, and yellow with ILI9341 `INVOFF`. A
user-supplied photo after `invertDisplay(true)` (`INVON`) showed the intended
red, green, and blue colors, a dark background and faceplate, and matching
direct-draw and buffered chip pairs. This panel-specific behavior was verified
on 2026-09-28; `lib/fnk0104b` now selects `INVON` during display setup.

## Device-verified microphone behavior

The Freenove Echo example assigns GPIO6 to I²S data input and GPIO8 to data
output. The audio diagnostic on this board returned a peak of 1 with the two
directions reversed. With GPIO6 as input, it detected spoken peaks above 1000
on 2026-09-29. The shared pin map now follows that verified direction.

## Device-verified OTA behavior

On 2026-09-29, the connected board was flashed over USB with the OTA demo
version 0.1.0. After publishing release v0.2.0 with an `ota.bin` asset, the
board connected to Wi-Fi, found the newer version, and waited for serial
confirmation. After confirmation, it downloaded all 972,128 bytes over HTTPS,
rebooted into version 0.2.0, reported `ota_boot_validation=accepted`, and then
reported `ota_status=up_to_date`. The OTA image was written to the alternate
app slot; no erase-all operation was used.
