#include <Arduino.h>
#include <fnk0104b/board.hpp>
#include <fnk0104b/status_led.hpp>
#include <ui/status_led.hpp>
void setup() {
  fnk0104b::board.begin();
  fnk0104b::status_led::write(0, 0, 0);
  Serial.println("rgb_diag patterns: solid / blink / breathe / rainbow, working / completion / attention, then off");
}
void loop() {
  using namespace ui::led;
  static uint32_t started = millis();
  static unsigned lastPhase = 99;
  static Color written{};
  const uint32_t elapsed = millis() - started;
  const unsigned phase = (elapsed / 2000) % 13;
  const State states[]{State::Working, State::Complete, State::Attention};
  const State state = phase < 12 ? states[phase / 4] : State::Off;
  const Pattern pattern = static_cast<Pattern>(phase % 4);
  if (phase != lastPhase) {
    lastPhase = phase;
    Serial.printf("rgb_diag phase=%u state=%u pattern=%u brightness=20 period_ms=2000\n", phase, unsigned(state), unsigned(pattern));
  }
  const auto color = frame(state, pattern, 20, 2000, elapsed % 2000);
  if (!(color == written)) {
    written = color; fnk0104b::status_led::write(color.red, color.green, color.blue);
  }
  delay(40);
}
