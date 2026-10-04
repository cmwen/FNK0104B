#if defined(FNK0104B_ENABLE_USB_AUDIO)
#include <Arduino.h>
#include <atomic>
#include <algorithm>
#include <esp_heap_caps.h>
#include <freertos/stream_buffer.h>
#include <fnk0104b/board.hpp>
#include <fnk0104b/audio_input.hpp>
#include <fnk0104b/usb_microphone.hpp>

namespace fnk0104b::audio_input {
namespace {
SemaphoreHandle_t peripheralMutex, audioMutex;
StaticStreamBuffer_t streamControl;
constexpr size_t kSpeechStorageBytes = 4097;
uint8_t* speechStorage = nullptr;  // CPU-only FIFO; I2S DMA remains internal.
StreamBufferHandle_t speechStream = nullptr;
StaticTask_t taskControl;
StackType_t taskStack[4096 / sizeof(StackType_t)];
std::atomic<bool> available{false}, speechSubscribed{false};
std::atomic<uint8_t> meter{0};
std::atomic<const char*> failure{"starting"};
void worker(void*) {
  int16_t pcm[256];
  uint32_t lastLog = millis(), lastRecovery = 0;
  for (;;) {
    size_t count = 0;
    xSemaphoreTake(audioMutex, portMAX_DELAY);
    if (!microphone.ready() && millis() - lastRecovery >= 1000) {
      lastRecovery = millis();
      xSemaphoreTake(peripheralMutex, portMAX_DELAY);
      const bool ok = microphone.begin();
      xSemaphoreGive(peripheralMutex);
      failure = ok ? "none" : "microphone_init_failed";
    }
    const bool ok = microphone.ready() && microphone.capture(pcm, 256, count, 1000) && count == 256;
    xSemaphoreGive(audioMutex);
    available = ok;
    if (ok) {
      failure = "none";
      int32_t peak = 0;
      for (size_t i = 0; i < count; ++i) peak = std::max(peak, std::abs(int32_t(pcm[i])));
      const uint8_t measured = std::min<int32_t>(100, peak / 64);
      meter = measured >= meter ? measured : (meter.load() * 3 + measured) / 4;
      usb_microphone::push(pcm, count);
      if (speechSubscribed) {
        // Speech failures/backpressure must never stall the USB microphone.
        if (xStreamBufferSpacesAvailable(speechStream) >= count * sizeof(int16_t))
          xStreamBufferSend(speechStream, pcm, count * sizeof(int16_t), 0);
      }
    } else {
      meter = 0;
      if (!strcmp(failure.load(), "none")) failure = "microphone_read_failed";
      vTaskDelay(pdMS_TO_TICKS(20));
    }
    if (millis() - lastLog >= 5000) {
      lastLog = millis();
      Serial.printf("monitor_audio ready=%d reason=%s level=%u usb_stream=%d usb_dropped=%lu heap=%u\n",
        available.load(), failure.load(), meter.load(), usb_microphone::streaming(),
        (unsigned long)usb_microphone::droppedSamples(), unsigned(ESP.getFreeHeap()));
    }
  }
}
}
bool begin(SemaphoreHandle_t peripheral, SemaphoreHandle_t audio) {
  peripheralMutex = peripheral; audioMutex = audio;
  if (!peripheral || !audio) { failure = "audio_mutex_missing"; return false; }
  speechStorage = static_cast<uint8_t*>(heap_caps_malloc(kSpeechStorageBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!speechStorage) { failure = "audio_buffer_failed"; return false; }
  speechStream = xStreamBufferCreateStatic(kSpeechStorageBytes, 1, speechStorage, &streamControl);
  if (!speechStream) { failure = "audio_stream_failed"; return false; }
  // Reserve I2S DMA before Wi-Fi/BLE and recognition model allocations.
  xSemaphoreTake(audioMutex, portMAX_DELAY); xSemaphoreTake(peripheralMutex, portMAX_DELAY);
  const bool ok = microphone.begin();
  xSemaphoreGive(peripheralMutex); xSemaphoreGive(audioMutex);
  failure = ok ? "none" : "microphone_init_failed";
  if (!xTaskCreateStaticPinnedToCore(worker, "monitor-audio", sizeof(taskStack), nullptr, 5, taskStack, &taskControl, 0)) {
    failure = "audio_task_failed"; return false;
  }
  return true; // A transient microphone failure is retried independently.
}
esp_err_t read(int16_t* samples, size_t count) {
  if (!speechStream || !samples || !count || count > (kSpeechStorageBytes - 1) / sizeof(int16_t)) return ESP_FAIL;
  speechSubscribed = true;
  const uint32_t started = millis();
  size_t received = 0;
  const size_t bytes = count * sizeof(int16_t);
  while (received < bytes && millis() - started < 1000) {
    received += xStreamBufferReceive(speechStream, reinterpret_cast<uint8_t*>(samples) + received,
      bytes - received, pdMS_TO_TICKS(100));
  }
  return received == bytes ? ESP_OK : ESP_FAIL;
}
bool ready() { return available.load(); }
uint8_t level() { return meter.load(); }
const char* error() { return failure.load(); }
}
#endif
