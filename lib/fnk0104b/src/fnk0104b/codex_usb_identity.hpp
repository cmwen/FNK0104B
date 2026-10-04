#pragma once
// The generic variant unconditionally sets PID=1001. Include it once before
// overriding identity; apply this header to USB.cpp, never edit installed cores.
#ifdef __cplusplus
#include <sdkconfig.h>
#include <pins_arduino.h>
#undef USB_VID
#undef USB_PID
#undef USB_PRODUCT
#undef USB_MANUFACTURER
#define USB_VID 0x303a
#define USB_PID 0x8360
#define USB_PRODUCT "Codex Micro"
#define USB_MANUFACTURER "Work Louder"
#if CONFIG_TINYUSB_AUDIO_ENABLED
#include "usb_audio_descriptor_compat.hpp"
#endif
#endif
