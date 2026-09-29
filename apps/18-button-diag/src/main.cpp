#include <Arduino.h>

#include <fnk0104b/board.hpp>
#include <fnk0104b/pins.hpp>

namespace {

constexpr char kFirmwareVersion[] = "0.1.0";
constexpr uint32_t kDebounceMs = 30;
bool stable_pressed = false;
bool last_reading_pressed = false;
uint32_t last_change_ms = 0;

}  // namespace

void setup() {
  fnk0104b::board.begin();
  fnk0104b::board.printStartupInfo("button-diagnostic", kFirmwareVersion);

  pinMode(fnk0104b::pins::expansion::gpio_14, INPUT_PULLUP);
  stable_pressed = digitalRead(fnk0104b::pins::expansion::gpio_14) == LOW;
  last_reading_pressed = stable_pressed;
  Serial.printf("button_gpio=%d active_level=LOW initial=%s\n",
                fnk0104b::pins::expansion::gpio_14,
                stable_pressed ? "pressed" : "released");
}

void loop() {
  const bool reading_pressed =
      digitalRead(fnk0104b::pins::expansion::gpio_14) == LOW;
  const uint32_t now = millis();

  if (reading_pressed != last_reading_pressed) {
    last_reading_pressed = reading_pressed;
    last_change_ms = now;
  }

  if (reading_pressed != stable_pressed && now - last_change_ms >= kDebounceMs) {
    stable_pressed = reading_pressed;
    Serial.printf("button=%s\n", stable_pressed ? "pressed" : "released");
  }

  delay(5);
}
