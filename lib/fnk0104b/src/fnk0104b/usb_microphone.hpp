#pragma once
#include <cstddef>
#include <cstdint>
namespace fnk0104b::usb_microphone {
bool begin();
bool streaming();
void push(const int16_t* samples, size_t count);
uint32_t droppedSamples();
}
