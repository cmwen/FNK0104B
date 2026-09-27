#include <Arduino.h>

#include <fnk0104b/board.hpp>

void setup() {
  fnk0104b::board.begin();
  fnk0104b::board.printPlaceholder("wifi", "0.1.0");
}

void loop() { delay(1000); }
