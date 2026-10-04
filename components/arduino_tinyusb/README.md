Arduino-as-IDF USB support follows Espressif's documented extra-component
workflow. `CMakeLists.txt`, `Kconfig.projbuild` and `include/tusb_config.h` are
from [esp32-arduino-lib-builder](https://github.com/espressif/esp32-arduino-lib-builder/tree/6671d0bd65cdb9d4cc1001b759e8610de945a8d5/components/arduino_tinyusb).
The configuration header retains its upstream MIT notice.

Local changes: fetch a SHA256-verified archive of an immutable TinyUSB commit with CMake FetchContent,
resolve the managed Arduino component name, default USB off outside the monitor,
and supply the USB selective-compilation switch missing from Arduino 3.3.12.
No installed framework is edited. TinyUSB sources remain in the ignored build
cache, with their upstream MIT license. First monitor build needs GitHub access.

TinyUSB pin: `c391fe9cb05abea9058ea3fc781c64e11bd057ab`.
Lib-builder's USB-host patches are unnecessary for this device-only integration.
CDC, HID and microphone-only USB Audio Class are enabled in the monitor defaults.
The local audio configuration uses mono PCM16 and a 4096-byte transmit FIFO to
accept the board's I2S capture blocks without blocking the capture worker.

Archive SHA256: `f96039275a6545febe16a3ac296f545f213445f31fbd77173e5bf9e8fe43ebdf`.
The unused S3 speaker OUT FIFO is 128 bytes; no speaker endpoint is exposed.
The board's UAC1 compatibility header adds an audio IAD for Windows grouping.
See [official references](../../knowledge/references.md#usb-microphone-recovery-references-2026-10-04).
