#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace codex_hid {
constexpr uint16_t kVid = 0x303a, kPid = 0x8360;
constexpr uint8_t kReportId = 6;
constexpr size_t kBodySize = 63, kChunkSize = 61, kMaxJson = 2048;
// Only the vendor collection is needed; standard keyboard/audio may be added later.
static constexpr uint8_t kDescriptor[] = {
  0x06,0x00,0xff, 0x09,0x01, 0xa1,0x01, 0x85,0x06,
  0x09,0x02, 0x15,0x00, 0x26,0xff,0x00, 0x75,0x08, 0x95,0x3f, 0x81,0x02,
  0x09,0x03, 0x91,0x02, 0xc0
};

// Reports arrive without their ID. Extract balanced JSON objects, including
// newline-free host calls; strings/escapes must not alter object depth.
class Decoder {
 public:
  using Handler = void (*)(const char*, size_t, void*);
  void reset() { used_ = depth_ = 0; quoted_ = escaped_ = dropping_ = false; }
  bool feed(const uint8_t* body, size_t length, Handler handler, void* context) {
    if (!body || length < 2 || length > kBodySize || body[0] != 2 ||
        body[1] > kChunkSize || body[1] > length - 2) {
      reset(); return false;
    }
    for (size_t i = 2; i < size_t(body[1]) + 2; ++i) {
      const char c = static_cast<char>(body[i]);
      if (!depth_) {
        if (c == ' ' || c == '\r' || c == '\n' || c == '\t') continue;
        if (c != '{') { reset(); return false; }
        depth_ = 1; buffer_[used_++] = c; continue;
      }
      if (!dropping_) {
        if (used_ == kMaxJson) { dropping_ = true; used_ = 0; }
        else buffer_[used_++] = c;
      }
      if (quoted_) {
        if (escaped_) escaped_ = false;
        else if (c == '\\') escaped_ = true;
        else if (c == '"') quoted_ = false;
      } else if (c == '"') quoted_ = true;
      else if (c == '{') ++depth_;
      else if (c == '}') {
        if (--depth_ == 0) {
          if (!dropping_) { buffer_[used_] = 0; handler(buffer_, used_, context); }
          reset();
        }
      }
      if (c == '\n' && depth_) { reset(); return false; }
    }
    return !dropping_;
  }
 private:
  char buffer_[kMaxJson + 1]{};
  size_t used_ = 0, depth_ = 0;
  bool quoted_ = false, escaped_ = false, dropping_ = false;
};

// Serialize one fragment of JSON + CRLF; offset advances only after USB accepts it.
inline size_t frame(const char* json, size_t size, size_t offset, uint8_t* body) {
  if (!json || !body || offset >= size + 2) return 0;
  std::memset(body, 0, kBodySize);
  const size_t count = size + 2 - offset < kChunkSize ? size + 2 - offset : kChunkSize;
  body[0] = 2; body[1] = static_cast<uint8_t>(count);
  for (size_t n = 0; n < count; ++n) {
    const size_t at = offset + n;
    body[n + 2] = at < size ? json[at] : at == size ? '\r' : '\n';
  }
  return count;
}
}  // namespace codex_hid
