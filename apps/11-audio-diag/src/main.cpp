#include <Arduino.h>

#include <fnk0104b/board.hpp>
#include <fnk0104b/pins.hpp>

namespace {

constexpr char kFirmwareVersion[] = "0.1.0";
constexpr size_t kCaptureSamples = 16000;
constexpr uint32_t kCaptureTimeoutMs = 2500;
int16_t samples[kCaptureSamples];
bool microphone_ready = false;

fnk0104b::MicrophoneConfig microphoneConfig() {
  return {
      fnk0104b::pins::audio::i2s_master_clock,
      fnk0104b::pins::audio::i2s_bit_clock,
      fnk0104b::pins::audio::i2s_word_select,
      fnk0104b::pins::audio::i2s_data_out,
      fnk0104b::pins::audio::i2s_data_in,
      fnk0104b::pins::audio::i2c_sda,
      fnk0104b::pins::audio::i2c_scl,
      static_cast<uint8_t>(fnk0104b::pins::audio::codec_i2c_address),
  };
}

}  // namespace

void setup() {
  fnk0104b::board.begin();
  fnk0104b::board.printStartupInfo("audio-diagnostic", kFirmwareVersion);
  microphone_ready = fnk0104b::microphone.begin(microphoneConfig());
  Serial.printf("microphone=%s sample_rate=16000 channels=1 bits=16\n",
                microphone_ready ? "ready" : "not_ready");
}

void loop() {
  if (!microphone_ready) {
    delay(1000);
    return;
  }

  size_t samples_captured = 0;
  const bool captured = fnk0104b::microphone.capture(
      samples, kCaptureSamples, samples_captured, kCaptureTimeoutMs);
  int32_t peak = 0;
  for (size_t i = 0; i < samples_captured; ++i) {
    const int32_t value = samples[i];
    const int32_t magnitude = value < 0 ? -value : value;
    if (magnitude > peak) peak = magnitude;
  }
  Serial.printf("microphone_capture=%s samples=%u signal=%s peak=%ld\n",
                captured && samples_captured == kCaptureSamples ? "complete"
                                                                : "timeout",
                static_cast<unsigned>(samples_captured),
                peak > 64 ? "detected" : "quiet", static_cast<long>(peak));
  delay(1000);
}
