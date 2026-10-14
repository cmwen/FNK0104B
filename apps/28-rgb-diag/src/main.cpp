#include <Arduino.h>
#include <fnk0104b/board.hpp>
#include <fnk0104b/status_led.hpp>
void setup() {
  fnk0104b::board.begin();
  fnk0104b::status_led::write(0, 0, 0);
  Serial.println("rgb_diag ready: green / yellow / red / off, two seconds each");
}
void loop() {
  static const uint8_t colors[][3] = {{0,16,0}, {16,16,0}, {16,0,0}, {0,0,0}};
  static unsigned index = 0;
  fnk0104b::status_led::write(colors[index][0], colors[index][1], colors[index][2]);
  Serial.printf("rgb_diag phase=%u\n", index);
  index = (index + 1) % 4;
  delay(2000);
}
