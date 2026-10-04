#pragma once
#include <cstddef>
#include <cstdint>
namespace fnk0104b::codex_usb {
struct Report { uint8_t body[63]; uint16_t length; uint32_t epoch; };
bool begin();
bool mounted();
uint32_t epoch();
bool receive(Report& report);
bool send(const uint8_t* body, size_t length);
uint32_t dropped();
}
