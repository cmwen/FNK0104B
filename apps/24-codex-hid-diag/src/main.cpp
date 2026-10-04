#include <Arduino.h>
#include <fnk0104b/board.hpp>
#include <codex_hid/backend.hpp>
void setup() {
  const bool ok = codex_hid::begin();
  fnk0104b::board.begin();
  fnk0104b::board.printStartupInfo("codex-hid-diag", "0.1.0");
  Serial.printf("codex_hid startup=%s; send t for AG00 press/release\n", ok ? "ready" : "failed");
}
void loop() {
  if (Serial.available() && Serial.read() == 't')
    Serial.printf("codex_hid input=%s\n", codex_hid::agent0Tap() ? "queued" : "unavailable");
  codex_hid::Status status;
  if (codex_hid::takeStatus(status)) Serial.printf("codex_hid application=received revision=%lu\n", (unsigned long)status.revision);
  delay(5);
}
