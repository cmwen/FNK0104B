#if defined(FNK0104B_ENABLE_CODEX_BLE)
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEHIDDevice.h>
#include <BLESecurity.h>
#include <atomic>
#include <freertos/queue.h>
#include <fnk0104b/codex_ble.hpp>
#include <codex_hid/wire.hpp>

namespace fnk0104b::codex_ble {
namespace {
BLEServer* owner = nullptr;
BLECharacteristic* input = nullptr;
QueueHandle_t rx = nullptr;
std::atomic<uint32_t> generation{0}, pairingUntil{0};
std::atomic<bool> linked{false}, encrypted{false}, accepted{false}, bondedPeer{false};
std::atomic<uint16_t> connection{0xffff};
uint8_t peerAddress[6]{}; // Accessed only by BLE callbacks.
// Retail BLE vendor report 6 has Input, Output and Feature items. This
// diagnostic exposes that control channel; no keyboard/audio is claimed.
uint8_t descriptor[] = {
  0x06,0x00,0xff, 0x09,0x01, 0xa1,0x01, 0x85,0x06,
  0x09,0x02, 0x15,0x00, 0x26,0xff,0x00, 0x75,0x08, 0x95,0x3f, 0x81,0x02,
  0x09,0x03, 0x91,0x02, 0x09,0x04, 0xb1,0x02, 0xc0
};
class Reports : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* characteristic) override {
    const size_t size = characteristic->getLength();
    if (!encrypted || !rx || size < 2 || size > codex_hid::kBodySize) return;
    Report report{}; report.length = size; report.epoch = generation.load();
    memcpy(report.body, characteristic->getData(), size);
    if (xQueueSend(rx, &report, 0) != pdTRUE) ++generation;
  }
  void onStatus(BLECharacteristic*, Status status, uint32_t) override {
    accepted = status == Status::SUCCESS_NOTIFY;
  }
};
class Security : public BLESecurityCallbacks {
  uint32_t onPassKeyRequest() override { return 0; }
  void onPassKeyNotify(uint32_t) override {}
  bool onConfirmPIN(uint32_t) override { return false; }
  bool onSecurityRequest() override { return bondedPeer || pairing(); }
  void onAuthenticationComplete(esp_ble_auth_cmpl_t result) override {
    if (!linked || memcmp(result.bd_addr, peerAddress, sizeof(peerAddress))) return;
    encrypted = result.success;
    if (result.success) { bondedPeer = true; pairingUntil = 0; }
    Serial.printf("codex_ble authentication=%s\n", result.success ? "bonded" : "failed");
  }
};
Reports reports;
Security security;
BLESecurity securityConfig;
}
bool begin(BLEServer* server) {
  if (owner) return true;
  if (!server) return false;
  rx = xQueueCreate(8, sizeof(Report));
  if (!rx) return false;
  owner = server;
  BLEDevice::setMTU(128);
  BLEDevice::setSecurityCallbacks(&security);
  securityConfig.setAuthenticationMode(ESP_LE_AUTH_REQ_SC_BOND);
  securityConfig.setCapability(ESP_IO_CAP_NONE);
  securityConfig.setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
  securityConfig.setRespEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
  auto* hid = new BLEHIDDevice(server);
  hid->manufacturer()->setValue("Work Louder");
  hid->pnp(2, codex_hid::kVid, codex_hid::kPid, 0x00b0);
  hid->hidInfo(0, 1);
  input = hid->inputReport(codex_hid::kReportId);
  input->setCallbacks(&reports);
  hid->outputReport(codex_hid::kReportId)->setCallbacks(&reports);
  hid->featureReport(codex_hid::kReportId)->setCallbacks(&reports);
  hid->reportMap(descriptor, sizeof(descriptor));
  hid->startServices();
  return true;
}
void connected(uint16_t id, const uint8_t* address) {
  if (!owner) return;
  if (linked) { owner->disconnect(id); return; } // One HID/settings peer at a time.
  if (address) memcpy(peerAddress, address, sizeof(peerAddress));
  connection = id; linked = true; encrypted = false; ++generation;
  bondedPeer = false;
  int count = esp_ble_get_bond_device_num();
  if (count > 0 && address) {
    auto* bonds = static_cast<esp_ble_bond_dev_t*>(malloc(count * sizeof(esp_ble_bond_dev_t)));
    if (bonds && esp_ble_get_bond_device_list(&count, bonds) == ESP_OK)
      for (int i = 0; i < count; ++i) if (!memcmp(bonds[i].bd_addr, address, 6)) bondedPeer = true;
    free(bonds);
  }
  Serial.printf("codex_ble connected bonded=%d\n", bondedPeer.load());
}
void disconnected(uint16_t id) {
  if (id != connection) return;
  linked = false; encrypted = false; connection = 0xffff; ++generation;
  if (rx) xQueueReset(rx);
}
void pairFor(uint32_t milliseconds) { pairingUntil = millis() + milliseconds; }
bool pairing() { const uint32_t until = pairingUntil; return until && int32_t(until - millis()) > 0; }
bool mounted() { return linked && encrypted; }
uint32_t epoch() { return generation; }
bool receive(Report& report) { return rx && xQueueReceive(rx, &report, 0) == pdTRUE; }
bool send(const uint8_t* body, size_t size) {
  if (!mounted() || !input || size != codex_hid::kBodySize || owner->getPeerMTU(connection) < size + 3) return false;
  accepted = false;
  input->setValue(const_cast<uint8_t*>(body), size);
  input->notify();
  return accepted.load();
}
}
#endif
