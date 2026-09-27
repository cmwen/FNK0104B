#include <Arduino.h>

#include <fnk0104b/board.hpp>

namespace {
fnk0104b::TouchPoint point{0, 0, false};
bool previous_pressed = false;
uint32_t last_error_at = 0;
}  // namespace

void setup() {
  fnk0104b::board.begin();
  fnk0104b::board.printStartupInfo("touch-diagnostic", "0.2.0");
  fnk0104b::touch.begin();
  Serial.println("touch_ready; tap the screen to print landscape coordinates");
}

void loop() {
  if (fnk0104b::touch.read(point)) {
    if (point.pressed && !previous_pressed) {
      Serial.printf("touch_x=%d touch_y=%d\n", point.x, point.y);
    }
    previous_pressed = point.pressed;
  } else if (millis() - last_error_at >= 1000) {
    Serial.println("touch_read=error");
    last_error_at = millis();
  }
  delay(12);
}
