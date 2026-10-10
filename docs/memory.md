# Memory and storage

## SRAM

The ESP32-S3 provides 512 KB internal SRAM according to the module specification. It is fast internal memory used by the CPU, stacks, static data, and parts of the runtime. It is not all available to an application; the framework and system reserve portions.

## PSRAM

The FNK0104B specification lists 8 MB OPI PSRAM. The PlatformIO build enables the ESP32-S3 OPI PSRAM mode. Runtime firmware still checks `ESP.getPsramSize()` and reports the detected amount instead of assuming initialization succeeded.

## Flash

The board specification lists 16 MB external SPI flash. Flash holds the bootloader, partition table, firmware, NVS, and any configured on-flash filesystem. PlatformIO uses the generic ESP32-S3 DevKitC-1 board definition because there is no exact FNK0104B board ID; the project overrides flash capacity and OPI PSRAM mode from the vendor specification.

PlatformIO explicitly selects its built-in `app3M_fat9M_16MB.csv`, matching the “16M Flash (3MB APP/9.9MB FATFS)” profile shown in Freenove's Arduino tutorial. It provides two 3 MiB OTA app slots, an approximately 9.9 MB FATFS partition, NVS, OTA metadata, and a coredump partition. The original factory partition table was not saved before this project uploaded its selected table to the connected board; exact factory equivalence is unknown. A PlatformIO upload writes the selected table. Back up data that matters before uploading to another board. The 3 MiB limit applies to each firmware image, so check image size as features grow.

## NVS

NVS is a small key/value store in an NVS partition, appropriate for configuration and small persistent state. It does not replace a general filesystem. Avoid storing credentials in source; load them from an untracked local file or a future provisioning flow.

## Optional filesystem

The selected layout provides FATFS for optional files. It is separate from NVS and from the removable SD card. Do not resize it or replace it with SPIFFS/LittleFS without documenting the new app/OTA capacities and data impact.

## SD card

The board has a removable MicroSD slot on four-bit SDIO. Use the verified `SD_MMC` mapping in `docs/pins.md`; it is separate from internal flash and useful for larger user files, logs, and media.

## Monitor 0.7.0 OTA layout

The Codex monitor now uses its own two 4 MiB application slots, 8 KiB OTA metadata
and a `0x7f0000`-byte speech-model region at `0x810000`. NVS stays at `0x9000`,
size `0x5000`. This replaces the monitor's previous single 6 MiB factory slot
and moves speech models from `0x610000`; a full one-time USB install is required.
It may overwrite old app/model/filesystem contents while keeping compatible NVS.
OTA updates only the inactive app; changed models/layouts require USB. See
[monitor OTA](monitor-ota.md) for the exact map and recovery behavior.

- Monitor 0.7.1 adds optional, unlabeled USB slot robots with stable random startup
  identities; Agent numbers remain the default. BLE settings v4 uses characteristic
  `4e4b0104-0006-4d20-8f4b-0104b0000001`, preserving v1–v3 reads/writes.
- Monitor speech packing now sorts model/file entries without changing weights.
  This removes CI/local raw-hash differences caused by upstream filesystem order;
  0.7.0 needs USB once to install the canonical baseline.
