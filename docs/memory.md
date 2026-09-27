# Memory and storage

## SRAM

The ESP32-S3 provides 512 KB internal SRAM according to the module specification. It is fast internal memory used by the CPU, stacks, static data, and parts of the runtime. It is not all available to an application; the framework and system reserve portions.

## PSRAM

The FNK0104B specification lists 8 MB OPI PSRAM. The PlatformIO build enables the ESP32-S3 OPI PSRAM mode. Runtime firmware still checks `ESP.getPsramSize()` and reports the detected amount instead of assuming initialization succeeded.

## Flash

The board specification lists 16 MB external SPI flash. Flash holds the bootloader, partition table, firmware, NVS, and any configured on-flash filesystem. PlatformIO uses the generic ESP32-S3 DevKitC-1 board definition because there is no exact FNK0104B board ID; the project overrides flash capacity and OPI PSRAM mode from the vendor specification.

PlatformIO explicitly selects its built-in `app3M_fat9M_16MB.csv`, matching the “16M Flash (3MB APP/9.9MB FATFS)” profile shown in Freenove's Arduino tutorial. It provides two 3 MiB OTA app slots, an approximately 9.9 MB FATFS partition, NVS, OTA metadata, and a coredump partition. The shipped factory partition table has not been read from a physical board, so exact factory equivalence is unknown. A PlatformIO upload writes the selected partition table; decide whether existing on-flash data matters before the first upload. The 3 MiB limit applies to each firmware image, so check image size as features grow.

## NVS

NVS is a small key/value store in an NVS partition, appropriate for configuration and small persistent state. It does not replace a general filesystem. Avoid storing credentials in source; load them from an untracked local file or a future provisioning flow.

## Optional filesystem

The selected layout provides FATFS for optional files. It is separate from NVS and from the removable SD card. Do not resize it or replace it with SPIFFS/LittleFS without documenting the new app/OTA capacities and data impact.

## SD card

The board has a removable MicroSD slot on four-bit SDIO. Use the verified `SD_MMC` mapping in `docs/pins.md`; it is separate from internal flash and useful for larger user files, logs, and media.
