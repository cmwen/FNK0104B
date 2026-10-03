#pragma once
#include <cstdint>

namespace speech {
// 16 kHz mono AFE output. Silence is already debounced by VADNet for 1 s.
class VoiceCaptureGate {
 public:
  bool feed(bool speech, uint32_t samples) {
    total_ += samples;
    voiced_ |= speech;
    silence_ = speech ? 0 : silence_ + samples;
    return (voiced_ && silence_ >= 16000) || (!voiced_ && total_ >= 96000) || total_ >= 144000;
  }
  bool voiced() const { return voiced_; }
 private:
  uint32_t total_ = 0, silence_ = 0;
  bool voiced_ = false;
};
}
