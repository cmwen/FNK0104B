#pragma once
#include <cstdint>

namespace codex_hid {
// Controls the board's outgoing audio, never infers Desktop session state.
class VoiceControls {
 public:
  static constexpr uint32_t kEndHoldMs = 1000;
  void configure(bool separateVoice) { enabled_ = separateVoice; reset(); }
  void reset() { ptt_ = voice_ = voiceHeld_ = afterTap_ = false; }
  bool voiceEnabled() const { return enabled_; }
  bool voiceOpen() const { return voice_; }
  bool audioOpen() const { return ptt_ || voice_; }
  bool pressPtt() {
    if (voice_ || voiceHeld_) return false;
    ptt_ = true; return true;
  }
  void releasePtt() { ptt_ = false; }
  bool pressVoice(uint32_t now) {
    if (!enabled_ || ptt_ || voiceHeld_) return false;
    // Close immediately when stopping. Open only after a short tap, so holding
    // an already-muted Voice key to end a chat never briefly reopens audio.
    afterTap_ = !voice_; voice_ = false;
    voiceHeld_ = true; started_ = now; return true;
  }
  bool tick(uint32_t now) {
    if (!voiceHeld_ || !afterTap_ || now - started_ < kEndHoldMs) return false;
    afterTap_ = false; return true;
  }
  void releaseVoice(uint32_t now) {
    if (!voiceHeld_) return;
    tick(now); voice_ = afterTap_; voiceHeld_ = false;
  }
 private:
  bool enabled_ = false, ptt_ = false, voice_ = false, voiceHeld_ = false, afterTap_ = false;
  uint32_t started_ = 0;
};
}
