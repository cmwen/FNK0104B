#include <Arduino.h>
#include <fnk0104b/board.hpp>
#include <codex_hid/backend.hpp>
void setup() {
  const bool ok = codex_hid::begin();
  fnk0104b::board.begin();
  fnk0104b::board.printStartupInfo("codex-hid-diag", "0.1.0");
  Serial.printf("codex_hid startup=%s; t=agent1, 1..6=agents, s=Send, f=Fast\n", ok ? "ready" : "failed");
}
void loop() {
  if (Serial.available()) {
    const int ch = Serial.read();
    const int key = ch == 't' ? 0 : ch >= '1' && ch <= '6' ? ch - '1' : ch == 's' ? 12 : ch == 'f' ? 6 : -1;
    if (key >= 0) Serial.printf("codex_hid key=%d input=%s\n", key,
      codex_hid::tap(key) ? "queued" : "unavailable");
  }
  codex_hid::Status status;
  if (codex_hid::takeStatus(status)) Serial.printf("codex_hid application=received revision=%lu\n", (unsigned long)status.revision);
  delay(5);
}
