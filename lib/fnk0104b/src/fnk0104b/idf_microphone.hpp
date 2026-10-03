#pragma once
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

namespace fnk0104b {
// ESP-IDF-only 16 kHz, mono, signed 16-bit input from the onboard ES8311.
esp_err_t beginIdfMicrophone();
esp_err_t readIdfMicrophone(int16_t* samples, size_t count);
}
