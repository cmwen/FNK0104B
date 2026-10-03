#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace recorder {
// Bounded subset: one complete packet per Ogg page, no chained streams.
constexpr size_t kMaxPacket = 1275;
constexpr size_t kMaxPage = 27 + 6 + kMaxPacket;
inline void putLe(uint8_t* out, uint64_t value, unsigned bytes) {
  for (unsigned i = 0; i < bytes; ++i) out[i] = value >> (8 * i);
}
inline uint64_t getLe(const uint8_t* in, unsigned bytes) {
  uint64_t value = 0;
  for (unsigned i = 0; i < bytes; ++i) value |= uint64_t(in[i]) << (8 * i);
  return value;
}
inline uint32_t oggCrc(const uint8_t* bytes, size_t count) {
  uint32_t crc = 0;
  for (size_t i = 0; i < count; ++i) {
    crc ^= uint32_t(i >= 22 && i < 26 ? 0 : bytes[i]) << 24;
    for (unsigned bit = 0; bit < 8; ++bit)
      crc = (crc << 1) ^ ((crc & 0x80000000u) ? 0x04c11db7u : 0);
  }
  return crc;
}
inline size_t makePage(uint8_t* out, size_t capacity, const uint8_t* packet,
                       size_t length, uint32_t serial, uint32_t sequence,
                       uint64_t granule, uint8_t flags) {
  if (!packet || !length || length > kMaxPacket || (flags & ~6)) return 0;
  const size_t segments = length / 255 + 1;
  const size_t total = 27 + segments + length;
  if (capacity < total) return 0;
  std::memset(out, 0, 27);
  std::memcpy(out, "OggS", 4);
  out[5] = flags;
  putLe(out + 6, granule, 8);
  putLe(out + 14, serial, 4);
  putLe(out + 18, sequence, 4);
  out[26] = segments;
  for (size_t i = 0; i < segments; ++i) out[27 + i] = i + 1 == segments ? length % 255 : 255;
  std::memcpy(out + 27 + segments, packet, length);
  putLe(out + 22, oggCrc(out, total), 4);
  return total;
}
struct Page {
  const uint8_t* packet = nullptr;
  size_t length = 0;
  uint32_t serial = 0, sequence = 0;
  uint64_t granule = 0;
  uint8_t flags = 0;
};
inline bool parsePage(const uint8_t* bytes, size_t count, Page& page) {
  if (count < 28 || std::memcmp(bytes, "OggS", 4) || bytes[4] || (bytes[5] & ~6)) return false;
  const unsigned segments = bytes[26];
  if (!segments || segments > 6 || count < 27u + segments) return false;
  size_t size = 0;
  for (unsigned i = 0; i < segments; ++i) {
    if ((i + 1 < segments && bytes[27 + i] != 255) ||
        (i + 1 == segments && bytes[27 + i] == 255)) return false;
    size += bytes[27 + i];
  }
  if (!size || size > kMaxPacket || count != 27 + segments + size ||
      getLe(bytes + 22, 4) != oggCrc(bytes, count)) return false;
  page = {bytes + 27 + segments, size, uint32_t(getLe(bytes + 14, 4)),
          uint32_t(getLe(bytes + 18, 4)), getLe(bytes + 6, 8), bytes[5]};
  return true;
}
// Native controls and automatic recording use elapsed audio, not UI wall time.
class RecordingGate {
 public:
  static constexpr unsigned kSilenceHoldMs = 2000;
  static constexpr unsigned kSilenceHoldSamples = 16000 * kSilenceHoldMs / 1000;
  void start() { samples_ = 0; silence_ = 0; speech_ = false; active_ = true; }
  void stop() { active_ = false; }
  bool active() const { return active_; }
  uint32_t samples() const { return samples_; }
  bool feed(bool speech, unsigned samples) {
    if (!active_) return false;
    samples_ += samples;
    speech_ |= speech;
    silence_ = speech ? 0 : silence_ + samples;
    // VADNet already debounces about 1 s silence; add 2 s here for
    // approximately 3 s total, resetting on resumed speech. Give wake users
    // 6 s to begin speaking. Bound each clip at 10 minutes.
    return (speech_ && silence_ >= kSilenceHoldSamples) || (!speech_ && samples_ >= 96000) || samples_ >= 9600000;
  }
 private:
  uint32_t samples_ = 0, silence_ = 0;
  bool speech_ = false, active_ = false;
};
}
