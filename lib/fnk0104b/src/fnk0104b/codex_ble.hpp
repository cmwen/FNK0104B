#pragma once
#include <fnk0104b/codex_usb.hpp>
class BLEServer;
namespace fnk0104b::codex_ble {
using Report = codex_usb::Report;
bool begin(BLEServer* server);
void connected(uint16_t id, const uint8_t* address);
void disconnected(uint16_t id);
void pairFor(uint32_t milliseconds = 60000);
bool pairing();
bool mounted();
uint32_t epoch();
bool receive(Report& report);
bool send(const uint8_t* body, size_t size);
}
