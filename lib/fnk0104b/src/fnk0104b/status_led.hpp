#pragma once
#include <cstdint>
namespace fnk0104b::status_led {
// Call from the main loop only. Board wiring and GRB order stay here.
void write(uint8_t red, uint8_t green, uint8_t blue);
}
