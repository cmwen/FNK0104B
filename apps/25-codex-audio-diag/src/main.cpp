#include <Arduino.h>
#include <fnk0104b/board.hpp>
#include <fnk0104b/audio_input.hpp>
#include <fnk0104b/usb_microphone.hpp>
#include <codex_hid/backend.hpp>
void setup() {
  codex_hid::begin();
  fnk0104b::board.begin(); fnk0104b::touch.begin();
  fnk0104b::board.printStartupInfo("codex-audio-diag", "0.1.0");
  const auto peripheral = xSemaphoreCreateMutex(), audio = xSemaphoreCreateMutex();
  const bool usb = fnk0104b::usb_microphone::begin();
  const bool input = fnk0104b::audio_input::begin(peripheral, audio);
  Serial.printf("audio_diag usb=%d capture=%d; p/r send Micro mic press/release\n", usb, input);
}
void loop() {
  if (Serial.available()) {
    const int key = Serial.read();
    if (key == 'p' || key == 'r') codex_hid::microphoneKey(key == 'p');
  }
  delay(10);
}
