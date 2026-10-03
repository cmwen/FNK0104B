#pragma once
#include <cstdint>

namespace speech {
constexpr uint32_t kVoiceSampleRate = 16000;
constexpr uint32_t kVoiceMaxSeconds = 30;
constexpr uint32_t kVoiceStartSeconds = 10;
constexpr uint32_t kVoiceSilenceSeconds = 4;
// 16 kHz mono AFE output. VADNet adds its existing 1 s silence debounce.
// Four more seconds allow a thinking pause; touch can still stop immediately.
inline uint8_t microphoneLevel(const int16_t* pcm, uint32_t count) {
  if (!pcm || !count) return 0;
  uint64_t sum = 0;
  for (uint32_t i = 0; i < count; ++i) {
    const int32_t sample = pcm[i];
    sum += sample < 0 ? -sample : sample;
  }
  uint32_t mean = sum / count;
  if (mean <= 32) return 0;
  uint8_t level = 0;
  for (mean /= 32; mean > 1 && level < 100; mean >>= 1) level += 14;
  return level > 100 ? 100 : level;
}

class VoiceCaptureGate {
 public:
  bool feed(bool speech, uint32_t samples) {
    total_ += samples;
    voiced_ |= speech;
    silence_ = speech ? 0 : silence_ + samples;
    return (voiced_ && silence_ >= kVoiceSampleRate * kVoiceSilenceSeconds) || (!voiced_ && total_ >= kVoiceSampleRate * kVoiceStartSeconds) || total_ >= kVoiceSampleRate * kVoiceMaxSeconds;
  }
  bool voiced() const { return voiced_; }
 private:
  uint32_t total_ = 0, silence_ = 0;
  bool voiced_ = false;
};
}
