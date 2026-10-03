// Optional interoperability check: compile against host libopus and decode
// output with FFmpeg. This exercises our muxer, pre-skip and final trim.
#include <opus.h>
#include <recorder/ogg.hpp>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <vector>
int main(int argc, char** argv) {
  if (argc != 3) return 1;
  const unsigned captured = std::strtoul(argv[2], nullptr, 10);
  if (!captured) return 2;
  int error = 0;
  OpusEncoder* encoder = opus_encoder_create(16000, 1, OPUS_APPLICATION_VOIP, &error);
  if (!encoder || error) return 3;
  opus_encoder_ctl(encoder, OPUS_SET_BITRATE(24000));
  opus_encoder_ctl(encoder, OPUS_SET_VBR(0));
  opus_encoder_ctl(encoder, OPUS_SET_COMPLEXITY(0));
  int lookahead = 0;
  opus_encoder_ctl(encoder, OPUS_GET_LOOKAHEAD(&lookahead));
  if (lookahead != 104) return 4;
  FILE* file = std::fopen(argv[1], "wb");
  if (!file) return 5;
  unsigned sequence = 0;
  auto page = [&](const uint8_t* packet, unsigned length, uint64_t granule, uint8_t flags) {
    uint8_t buffer[recorder::kMaxPage];
    const size_t count = recorder::makePage(buffer, sizeof(buffer), packet, length, 42, sequence++, granule, flags);
    recorder::Page parsed;
    if (!count || !recorder::parsePage(buffer, count, parsed) || std::fwrite(buffer, 1, count, file) != count) std::exit(6);
  };
  uint8_t head[19] = {}; std::memcpy(head, "OpusHead", 8);
  head[8] = 1; head[9] = 1;
  recorder::putLe(head + 10, lookahead * 3, 2); recorder::putLe(head + 12, 16000, 4);
  page(head, 19, 0, 2);
  uint8_t tags[16] = {}; std::memcpy(tags, "OpusTags", 8); page(tags, 16, 0, 0);
  for (unsigned offset = 0; offset < captured + lookahead; offset += 320) {
    int16_t pcm[320] = {};
    for (unsigned i = 0; i < 320 && offset + i < captured; ++i)
      pcm[i] = std::lround(10000 * std::sin(2 * 3.141592653589793 * 440 * (offset + i) / 16000));
    uint8_t packet[recorder::kMaxPacket];
    const int length = opus_encode(encoder, pcm, 320, packet, sizeof(packet));
    if (length <= 0) return 7;
    const bool last = offset + 320 >= captured + lookahead;
    page(packet, length, last ? captured * 3ull + lookahead * 3 : (offset + 320) * 3ull, last ? 4 : 0);
  }
  opus_encoder_destroy(encoder);
  return std::fclose(file) ? 8 : 0;
}
