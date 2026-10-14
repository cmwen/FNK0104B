#if defined(FNK0104B_ENABLE_STATUS_LED)
#include <Arduino.h>
#include <fnk0104b/pins.hpp>
#include <fnk0104b/status_led.hpp>
namespace fnk0104b::status_led {
void write(uint8_t red, uint8_t green, uint8_t blue) {
  rgbLedWriteOrdered(pins::rgb::data, LED_COLOR_ORDER_GRB, red, green, blue);
}
}
#endif
