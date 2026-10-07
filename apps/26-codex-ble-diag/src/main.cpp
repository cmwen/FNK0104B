#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <fnk0104b/board.hpp>
#include <fnk0104b/codex_ble.hpp>
#include <codex_hid/backend.hpp>
#include <codex_hid/ble_backend.hpp>
class Connections : public BLEServerCallbacks {
  void onConnect(BLEServer*, esp_ble_gatts_cb_param_t* p) override {
    fnk0104b::codex_ble::connected(p->connect.conn_id, p->connect.remote_bda);
  }
  void onDisconnect(BLEServer*, esp_ble_gatts_cb_param_t* p) override {
    fnk0104b::codex_ble::disconnected(p->disconnect.conn_id);
    BLEDevice::startAdvertising();
  }
};
void setup() {
  codex_hid::begin(); fnk0104b::board.begin();
  fnk0104b::board.printStartupInfo("codex-ble-diag", "0.1.0");
  BLEDevice::init("Codex Micro");
  auto* server = BLEDevice::createServer();
  server->setCallbacks(new Connections());
  const bool ok = codex_hid::ble::begin() && fnk0104b::codex_ble::begin(server);
  auto* advertising = BLEDevice::getAdvertising();
  advertising->setAppearance(0x03c0);
  advertising->addServiceUUID(BLEUUID(uint16_t(0x1812)));
  advertising->start(); fnk0104b::codex_ble::pairFor();
  Serial.printf("codex_ble startup=%d pairing=60s; b=reopen pairing, 1..6=agent taps\n", ok);
}
void loop() {
  if (Serial.available()) {
    const int key = Serial.read();
    if (key == 'b') fnk0104b::codex_ble::pairFor();
    else if (key >= '1' && key <= '6') codex_hid::tap(key - '1');
  }
  delay(10);
}
