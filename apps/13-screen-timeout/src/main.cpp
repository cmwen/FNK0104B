#include <Arduino.h>

#include <fnk0104b/board.hpp>

namespace {
constexpr uint32_t kIdleTimeoutMs = 60UL * 1000UL;
constexpr uint32_t kTouchPollMs = 20;

fnk0104b::TouchPoint touch_point{0, 0, false};
uint32_t last_activity_at = 0;
uint32_t last_touch_poll_at = 0;
uint32_t last_touch_error_at = 0;
bool screen_on = true;
bool previous_touch = false;

void drawStatus() {
  TFT_eSPI& tft = fnk0104b::display.driver();
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("SCREEN TIMEOUT TEST", tft.width() / 2, 55, 2);
  tft.drawString("Backlight turns off", tft.width() / 2, 105, 2);
  tft.drawString("after 1 minute idle", tft.width() / 2, 130, 2);
  tft.drawString("Touch anywhere to wake", tft.width() / 2, 185, 2);
}

void wakeScreen() {
  fnk0104b::display.setBacklight(true);
  screen_on = true;
  last_activity_at = millis();
  drawStatus();
  Serial.println("screen=on reason=touch");
}
}  // namespace

void setup() {
  fnk0104b::board.begin();
  fnk0104b::board.printStartupInfo("screen-timeout-diagnostic", "0.1.0");
  fnk0104b::display.begin(1);
  drawStatus();

  const bool touch_ready = fnk0104b::touch.begin();
  Serial.printf("touch=%s\n", touch_ready ? "ready" : "not_found");
  Serial.println("screen_timeout_seconds=60");
  Serial.println("touch the screen to wake the backlight");
  last_activity_at = millis();
  last_touch_poll_at = last_activity_at;
}

void loop() {
  const uint32_t now = millis();
  if (now - last_touch_poll_at >= kTouchPollMs) {
    last_touch_poll_at = now;
    if (fnk0104b::touch.read(touch_point)) {
      if (touch_point.pressed) {
        last_activity_at = now;
        if (!screen_on) {
          wakeScreen();
        } else if (!previous_touch) {
          Serial.printf("touch_x=%d touch_y=%d\n", touch_point.x,
                        touch_point.y);
        }
      }
      previous_touch = touch_point.pressed;
    } else if (now - last_touch_error_at >= 1000) {
      Serial.println("touch_read=error");
      last_touch_error_at = now;
      previous_touch = false;
    }
  }

  // Touch reads take several I²C transactions. Refresh after polling because
  // wakeScreen() may have recorded a newer timestamp during that work.
  const uint32_t time_now = millis();
  if (screen_on && time_now - last_activity_at >= kIdleTimeoutMs) {
    fnk0104b::display.setBacklight(false);
    screen_on = false;
    Serial.println("screen=off reason=idle_timeout");
  }
  delay(1);
}
