#include <Arduino.h>
#include <fnk0104b/board.hpp>
#include <fnk0104b/keyboard.hpp>

void setup() {
  fnk0104b::keyboard.begin();
  fnk0104b::board.begin();
  fnk0104b::board.printStartupInfo("hid-diag", "0.1.1");
  Serial.println("Send t over serial to type 123 in the focused host app. No automatic typing.");
}
void loop() {
  if (Serial.available() && Serial.read() == 't') {
    Serial.printf("hid_test=%s\n", fnk0104b::keyboard.text("123") ? "sent" : "failed");
  }
  static uint32_t last = 0;
  if (millis() - last >= 2000) {
    last = millis();
    Serial.printf("hid_ready=%d endpoint_ready=%d num_lock=%d\n", fnk0104b::keyboard.ready(), fnk0104b::keyboard.endpointReady(), fnk0104b::keyboard.numLock());
  }
  delay(5);
}
