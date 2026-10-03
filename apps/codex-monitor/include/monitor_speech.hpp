#pragma once
#include <cstddef>
#include <cstdint>
#include <atomic>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

namespace monitor_speech {
enum class Event : uint8_t { Wake = 0, StartListening = 1, GoBack,
                             ShowStatus, ScreenOn, ScreenOff, Timeout, Error };
bool begin(SemaphoreHandle_t peripheral_mutex, SemaphoreHandle_t audio_mutex, std::atomic<bool>* voice_busy);
bool poll(Event& event);
bool ready();
bool listening();
bool speech();
uint8_t level();
void quietFor(uint32_t milliseconds);
// Copies continuous AFE output; the feed task remains the only microphone reader.
bool capture(int16_t* pcm, size_t capacity, size_t& captured, const std::atomic<bool>& stop);
}
