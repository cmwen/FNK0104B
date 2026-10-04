---
title: "NVS, flash, PSRAM & SD"
summary: "Understand which data survives a reboot, an update or a power loss."
status: Implemented
---

## Is NVS where Wi-Fi is saved?
Yes. **NVS means nonvolatile storage**: small persistent key/value data in the board's flash. The Wi-Fi stack stores provisioned credentials there, and the monitor stores volume and idle preferences. It is appropriate for settings, not large recordings. The separate `nvs` app is still a placeholder; that does not mean other apps are not using NVS.

| Storage | What it is good for | Survives power off? |
| --- | --- | --- |
| NVS in flash | Saved Wi-Fi and small preferences | Yes, unless erased or overwritten |
| Other flash partitions | Firmware, speech models, app-specific data | Yes, with layout-dependent updates |
| Internal RAM / 8 MB PSRAM | Buffers, graphics and temporary audio | No |
| Removable microSD | Recordings and files | Yes, after successful writes |

The board has 16 MB flash and 8 MB PSRAM. These are different resources. Free PSRAM does not solve every internal DMA or task-stack allocation: monitor integration exposed failures that required early microphone DMA allocation and a reserved status-feed stack.

## Updates need compatible boundaries
Leaving **Erase device** unchecked can preserve settings when the new firmware uses compatible NVS boundaries. It is not a universal guarantee. The current monitor preserves the Arduino 20 KiB NVS region but replaces OTA/FATFS storage with speech models. The standalone speech diagnostic and recorder use a different, 24 KiB NVS boundary; cross-layout interpretation is not guaranteed. Switching layouts can overwrite old app/data regions without a full-chip erase.

## Files on SD
The verified interface is four-bit SDIO/SD_MMC. The file manager views existing files without formatting. The recorder writes Ogg Opus to `/recordings/`, preserving existing names and keeping incomplete files marked `.part`. Never assume removing power halfway through a write leaves a valid file.

## Brief your coding agent
> Save a small user preference in NVS with a sensible default and validation. Write only when it changes. Explain update compatibility. Keep recordings on SD and preserve existing files; never format automatically.

## Sources
[Espressif NVS guide](https://docs.espressif.com/projects/esp-idf/en/v5.5.5/esp32s3/api-reference/storage/nvs_flash.html). Layout and device evidence are recorded in the monitor and recorder READMEs.
