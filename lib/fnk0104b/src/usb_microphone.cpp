#if defined(FNK0104B_ENABLE_USB_AUDIO)
#include <Arduino.h>
#include <USBAudioCard.h>
#include <USB.h>
#include <tusb.h>
#include <atomic>
#include <fnk0104b/usb_microphone.hpp>

namespace fnk0104b::usb_microphone {
namespace {
// Construction registers the descriptor before Arduino auto-starts USB.
USBAudioCard microphone(16000, UAC_BPS_16, UAC_SPK_NONE, UAC_MIC_MONO);
std::atomic<bool> enabled{false};
std::atomic<uint32_t> drops{0};
void usbEvent(void*, esp_event_base_t, int32_t id, void*) {
  if (id == ARDUINO_USB_STOPPED_EVENT) enabled = false;
}
void event(void*, esp_event_base_t, int32_t id, void* data) {
  auto* audio = static_cast<arduino_usb_audio_card_event_data_t*>(data);
  if (id == ARDUINO_USB_AUDIO_CARD_INTERFACE_ENABLE_EVENT && audio->interface_enable.interface == UAC_INTERFACE_MIC) {
    enabled = audio->interface_enable.enable;
    Serial.printf("usb_mic stream=%s rate=16000 channels=1 bits=16\n", enabled ? "on" : "off");
  }
}
}
bool begin() { USB.onEvent(usbEvent); microphone.onEvent(event); return microphone.begin(); }
bool streaming() { return enabled.load() && tud_mounted(); }
uint32_t droppedSamples() { return drops.load(); }
void push(const int16_t* samples, size_t count) {
  if (!streaming() || !samples || count > 1024) return;
  auto* fifo = tud_audio_get_ep_in_ff();
  const uint16_t bytes = count * sizeof(int16_t);
  // Never block the I2S producer and never truncate in the middle of a sample.
  if (!fifo || tu_fifo_remaining(fifo) < bytes) { drops += count; return; }
  const size_t sent = microphone.write(samples, bytes);
  if (sent != bytes) drops += (bytes - sent) / sizeof(int16_t);
}
}
#endif
