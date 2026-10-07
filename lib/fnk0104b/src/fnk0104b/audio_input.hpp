#pragma once
#include <cstddef>
#include <cstdint>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <esp_err.h>
namespace fnk0104b::audio_input {
// One physical reader supplies USB PCM and the existing speech frontend.
bool begin(SemaphoreHandle_t peripheral, SemaphoreHandle_t audio);
esp_err_t read(int16_t* samples, size_t count);
// Call after the speech reader has paused; raw USB capture continues.
void unsubscribeSpeech();
bool ready();
uint8_t level();
const char* error();
}
