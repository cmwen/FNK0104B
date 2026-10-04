#pragma once
#if defined(FNK0104B_ENABLE_USB_HID)
#include <stdint.h>
namespace fnk0104b {
class KeyboardSupport {
 public:
  bool begin();
  bool ready();
  bool endpointReady();
  bool tap(uint8_t usage, uint8_t modifiers = 0);
  bool text(const char* ascii);
  bool releaseAll();
  bool numLock() const;
};
extern KeyboardSupport keyboard;
}
#endif
