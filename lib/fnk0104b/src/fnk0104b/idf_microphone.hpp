#pragma once
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

namespace fnk0104b {
// ESP-IDF-only 16 kHz, mono, signed 16-bit input from the onboard ES8311.
esp_err_t beginIdfMicrophone();
// Full-duplex 16 kHz audio, using the same clocks and shared codec bus.
esp_err_t beginIdfAudio();
esp_err_t setIdfSpeakerEnabled(bool enabled);
esp_err_t writeIdfSpeaker(const int16_t* samples, size_t count);
// Touch attaches to the existing audio I2C bus; never recreates that bus.
struct IdfTouchPoint { int x = 0; int y = 0; bool pressed = false; };
esp_err_t beginIdfTouch();
esp_err_t readIdfTouch(IdfTouchPoint& point);
esp_err_t readIdfMicrophone(int16_t* samples, size_t count);
}
