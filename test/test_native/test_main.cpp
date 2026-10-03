#include <unity.h>
#include <initializer_list>

#include <string.h>

#include <locallink/protocol.hpp>
#include <locallink/sse.hpp>
#include <ui/avatar_assets.hpp>
#include <ui/idle_timer.hpp>
#include <recorder/ogg.hpp>
#include "../../lib/speech/src/voice_capture_gate.hpp"

void test_sse_fragmented_status_comments_and_unknown_events() {
  locallink::StatusEventParser<256> parser;
  using Result = decltype(parser)::Result;
  const char* wire = ": heartbeat\r\n\r\nevent: unknown\ndata: ignore\n\n"
                     "event: status\r\ndata: {\r\ndata: \"total_agents\":0}\r\n\r\n";
  unsigned events = 0;
  for (const char* c = wire; *c; ++c) {
    const Result result = parser.feed(*c);
    TEST_ASSERT_TRUE(result != Result::Overflow);
    if (result == Result::Status) {
      ++events;
      TEST_ASSERT_EQUAL_STRING("{\n\"total_agents\":0}", parser.data());
    }
  }
  TEST_ASSERT_EQUAL_UINT32(1, events);
  // A new event must not contain data from the previous event.
  const char* next = "event: status\ndata: {}\n\n";
  for (const char* c = next; *c; ++c)
    if (parser.feed(*c) == Result::Status) TEST_ASSERT_EQUAL_STRING("{}", parser.data());
}

void test_sse_never_delivers_incomplete_or_oversized_events() {
  locallink::StatusEventParser<32> parser;
  using Result = decltype(parser)::Result;
  const char* incomplete = "event: status\ndata: {\"x\":";
  for (const char* c = incomplete; *c; ++c) TEST_ASSERT_TRUE(parser.feed(*c) == Result::None);
  bool overflow = false;
  for (unsigned n = 0; n < 40; ++n) {
    const Result result = parser.feed('x');
    TEST_ASSERT_TRUE(result != Result::Status);
    if (result == Result::Overflow) { overflow = true; break; }
  }
  TEST_ASSERT_TRUE(overflow);
  locallink::StatusEventParser<32> multiline;
  const char* wire = "event: status\ndata: 12345678901234567890\ndata: 12345678901234567890\n\n";
  overflow = false;
  for (const char* c = wire; *c; ++c) {
    const Result result = multiline.feed(*c);
    if (result == Result::Overflow) { overflow = true; break; }
    TEST_ASSERT_TRUE(result != Result::Status);
  }
  TEST_ASSERT_TRUE(overflow);
}

void test_monitor_timeout_starts_after_active_work_finishes() {
  ui::IdleTimer timer;
  timer.activity(100);
  TEST_ASSERT_FALSE(timer.expired(60100, 60000, true));
  TEST_ASSERT_FALSE(timer.expired(180100, 60000, true));
  TEST_ASSERT_FALSE(timer.expired(240099, 60000, false));
  TEST_ASSERT_TRUE(timer.expired(240100, 60000, false));
  // Work arriving after sleep keeps the display awake and starts a fresh interval.
  TEST_ASSERT_FALSE(timer.expired(300100, 60000, true));
  TEST_ASSERT_FALSE(timer.expired(360099, 60000, false));
}

void test_monitor_touch_and_timeout_setting() {
  ui::IdleTimer timer;
  timer.activity(100);
  TEST_ASSERT_TRUE(timer.expired(60100, 60000, false));
  timer.activity(60100);
  TEST_ASSERT_FALSE(timer.expired(120099, 60000, false));
  TEST_ASSERT_TRUE(timer.expired(120100, 60000, false));
  TEST_ASSERT_FALSE(timer.expired(120100, 120000, false));
}

void test_monitor_timeout_across_millis_wrap() {
  ui::IdleTimer timer;
  timer.activity(UINT32_MAX - 29999);
  TEST_ASSERT_FALSE(timer.expired(29999, 60000, false));
  TEST_ASSERT_TRUE(timer.expired(30000, 60000, false));
}

void test_avatar_rgb565_channel_order() {
  TEST_ASSERT_EQUAL_HEX16(0xF800, ui::avatar::detail::rgb(255, 0, 0));
  TEST_ASSERT_EQUAL_HEX16(0x07E0, ui::avatar::detail::rgb(0, 255, 0));
  TEST_ASSERT_EQUAL_HEX16(0x001F, ui::avatar::detail::rgb(0, 0, 255));
}

void test_avatar_identity_and_state_are_independent() {
  const uint32_t ada = ui::avatar::hashId("Ada");
  TEST_ASSERT_EQUAL_HEX32(0x9ac3a55b, ada);

  ui::avatar::Canvas idle = {};
  ui::avatar::Canvas thinking = {};
  ui::avatar::Canvas another = {};
  ui::avatar::render(idle, ada, ui::avatar::Mood::Idle, 1);
  ui::avatar::render(thinking, ada, ui::avatar::Mood::Thinking, 1);
  ui::avatar::render(another, ui::avatar::hashId("Dex"),
                     ui::avatar::Mood::Idle, 1);
  TEST_ASSERT_NOT_EQUAL(0, memcmp(idle.pixels, thinking.pixels, sizeof(idle.pixels)));
  TEST_ASSERT_NOT_EQUAL(0, memcmp(idle.pixels, another.pixels, sizeof(idle.pixels)));
}

void test_avatar_animation_changes_frame() {
  ui::avatar::Canvas first = {};
  ui::avatar::Canvas moved = {};
  const uint32_t id = ui::avatar::hashId("Ada");
  ui::avatar::render(first, id, ui::avatar::Mood::Thinking, 1);
  ui::avatar::render(moved, id, ui::avatar::Mood::Thinking, 5);
  TEST_ASSERT_NOT_EQUAL(0, memcmp(first.pixels, moved.pixels, sizeof(first.pixels)));
}

void test_native_runner_smoke() {
  TEST_ASSERT_EQUAL_INT(42, 6 * 7);
}

void test_selects_matching_dns_sd_service_and_txt_path() {
  locallink::ServiceRecord records[2] = {};
  strcpy(records[0].instance, "Other Speech Recognition");
  strcpy(records[0].host, "other.local.");
  strcpy(records[0].path, "/wrong");
  records[0].port = 8080;
  strcpy(records[1].instance, "LocalLink Speech Recognition");
  strcpy(records[1].host, "speech-locallink.local.");
  strcpy(records[1].path, "/v1/audio/transcriptions");
  records[1].port = 8081;

  locallink::ServiceRecord fallback = {};
  locallink::Endpoint selected = {};
  TEST_ASSERT_TRUE(locallink::selectEndpoint(
      records, 2, "LocalLink Speech Recognition", fallback, selected));
  TEST_ASSERT_EQUAL_STRING("speech-locallink.local", selected.host);
  TEST_ASSERT_EQUAL_STRING("/v1/audio/transcriptions", selected.path);
  TEST_ASSERT_EQUAL_UINT16(8081, selected.port);
  TEST_ASSERT_FALSE(selected.from_fallback);
}

void test_endpoint_falls_back_only_when_configured() {
  locallink::ServiceRecord fallback = {};
  strcpy(fallback.host, "192.168.1.32");
  strcpy(fallback.path, "/v1/audio/transcriptions");
  fallback.port = 8081;
  locallink::Endpoint selected = {};
  TEST_ASSERT_TRUE(locallink::selectEndpoint(
      nullptr, 0, "LocalLink Speech Recognition", fallback, selected));
  TEST_ASSERT_TRUE(selected.from_fallback);
  TEST_ASSERT_EQUAL_STRING("192.168.1.32", selected.host);
  TEST_ASSERT_EQUAL_STRING("/v1/audio/transcriptions", selected.path);
  TEST_ASSERT_EQUAL_UINT16(8081, selected.port);

  memset(&fallback, 0, sizeof(fallback));
  TEST_ASSERT_FALSE(locallink::selectEndpoint(
      nullptr, 0, "LocalLink Speech Recognition", fallback, selected));
}

void test_service_instance_match_is_exact_and_configured() {
  locallink::ServiceRecord record = {};
  strcpy(record.instance, "LocalLink Speech Recognition");
  strcpy(record.host, "speech-locallink.local");
  strcpy(record.path, "/v1/audio/transcriptions");
  record.port = 8081;

  locallink::ServiceRecord fallback = {};
  locallink::Endpoint selected = {};
  TEST_ASSERT_FALSE(locallink::selectEndpoint(
      &record, 1, "Speech Recognition", fallback, selected));
  TEST_ASSERT_TRUE(locallink::selectEndpoint(
      &record, 1, "LocalLink Speech Recognition", fallback, selected));
  TEST_ASSERT_FALSE(selected.from_fallback);
  TEST_ASSERT_EQUAL_STRING("speech-locallink.local", selected.host);
}

void test_wav_header_is_mono_16_bit_pcm() {
  uint8_t header[44] = {};
  TEST_ASSERT_TRUE(locallink::writeWavHeader(header, 32000, 16000));
  TEST_ASSERT_EQUAL_MEMORY("RIFF", header, 4);
  TEST_ASSERT_EQUAL_UINT32(32036, static_cast<uint32_t>(header[4]) |
                                    (static_cast<uint32_t>(header[5]) << 8) |
                                    (static_cast<uint32_t>(header[6]) << 16) |
                                    (static_cast<uint32_t>(header[7]) << 24));
  TEST_ASSERT_EQUAL_MEMORY("WAVEfmt ", header + 8, 8);
  TEST_ASSERT_EQUAL_UINT16(1, static_cast<uint16_t>(header[22]));
  TEST_ASSERT_EQUAL_UINT32(16000, static_cast<uint32_t>(header[24]) |
                                     (static_cast<uint32_t>(header[25]) << 8) |
                                     (static_cast<uint32_t>(header[26]) << 16) |
                                     (static_cast<uint32_t>(header[27]) << 24));
  TEST_ASSERT_EQUAL_MEMORY("data", header + 36, 4);
  TEST_ASSERT_EQUAL_UINT32(32000, static_cast<uint32_t>(header[40]) |
                                     (static_cast<uint32_t>(header[41]) << 8) |
                                     (static_cast<uint32_t>(header[42]) << 16) |
                                     (static_cast<uint32_t>(header[43]) << 24));
  TEST_ASSERT_FALSE(locallink::writeWavHeader(header, 3, 16000));
}

void test_wav_header_matches_early_stopped_capture_length() {
  constexpr uint32_t partial_samples = 47213;
  constexpr uint32_t pcm_bytes = partial_samples * sizeof(int16_t);
  uint8_t header[44] = {};
  TEST_ASSERT_TRUE(locallink::writeWavHeader(header, pcm_bytes, 16000));
  const uint32_t riff_size = static_cast<uint32_t>(header[4]) |
                             (static_cast<uint32_t>(header[5]) << 8) |
                             (static_cast<uint32_t>(header[6]) << 16) |
                             (static_cast<uint32_t>(header[7]) << 24);
  const uint32_t data_size = static_cast<uint32_t>(header[40]) |
                             (static_cast<uint32_t>(header[41]) << 8) |
                             (static_cast<uint32_t>(header[42]) << 16) |
                             (static_cast<uint32_t>(header[43]) << 24);
  TEST_ASSERT_EQUAL_UINT32(pcm_bytes + 36, riff_size);
  TEST_ASSERT_EQUAL_UINT32(pcm_bytes, data_size);
}

void test_multipart_uses_file_field_and_closing_boundary() {
  char prefix[192] = {};
  char suffix[64] = {};
  const size_t prefix_size =
      locallink::writeMultipartPrefix("----unit-test", prefix, sizeof(prefix));
  const size_t suffix_size =
      locallink::writeMultipartSuffix("----unit-test", suffix, sizeof(suffix));
  TEST_ASSERT_GREATER_THAN_UINT32(0, prefix_size);
  TEST_ASSERT_GREATER_THAN_UINT32(0, suffix_size);
  TEST_ASSERT_NOT_NULL(strstr(prefix, "name=\"file\""));
  TEST_ASSERT_NOT_NULL(strstr(prefix, "Content-Type: audio/wav"));
  TEST_ASSERT_EQUAL_STRING("\r\n------unit-test--\r\n", suffix);
}


void test_recorder_ogg_boundaries_and_corruption() {
  uint8_t packet[recorder::kMaxPacket];
  for (unsigned i = 0; i < sizeof(packet); ++i) packet[i] = i;
  uint8_t page[recorder::kMaxPage];
  for (unsigned size : {1u, 254u, 255u, 256u, 1275u}) {
    const size_t length = recorder::makePage(page, sizeof(page), packet, size, 123, 7, 123456789012ull, 4);
    TEST_ASSERT_GREATER_THAN_UINT32(size, length);
    recorder::Page parsed;
    TEST_ASSERT_TRUE(recorder::parsePage(page, length, parsed));
    TEST_ASSERT_EQUAL_UINT32(size, parsed.length);
    TEST_ASSERT_EQUAL_UINT32(123, parsed.serial);
    TEST_ASSERT_EQUAL_UINT32(7, parsed.sequence);
    TEST_ASSERT_EQUAL_UINT64(123456789012ull, parsed.granule);
    TEST_ASSERT_EQUAL_MEMORY(packet, parsed.packet, size);
    TEST_ASSERT_FALSE(recorder::parsePage(page, length - 1, parsed));
    page[length - 1] ^= 1;
    TEST_ASSERT_FALSE(recorder::parsePage(page, length, parsed));
  }
  TEST_ASSERT_EQUAL_UINT32(0, recorder::makePage(page, sizeof(page), packet, 0, 1, 0, 0, 0));
  TEST_ASSERT_EQUAL_UINT32(0, recorder::makePage(page, sizeof(page), packet, 1276, 1, 0, 0, 0));
  TEST_ASSERT_EQUAL_UINT32(0, recorder::makePage(page, 28, packet, 255, 1, 0, 0, 0));
}
void test_recorder_gate_silence_grace_reset_and_limit() {
  recorder::RecordingGate gate;
  TEST_ASSERT_FALSE(gate.feed(true, 512));
  gate.start();
  TEST_ASSERT_FALSE(gate.feed(false, 95999));
  TEST_ASSERT_TRUE(gate.feed(false, 1));
  gate.start();
  TEST_ASSERT_FALSE(gate.feed(true, 512));
  TEST_ASSERT_FALSE(gate.feed(false, 31999));
  TEST_ASSERT_FALSE(gate.feed(true, 512));
  TEST_ASSERT_FALSE(gate.feed(false, 31999));
  TEST_ASSERT_TRUE(gate.feed(false, 1));
  gate.stop();
  TEST_ASSERT_FALSE(gate.active());
  TEST_ASSERT_FALSE(gate.feed(true, 512));
  gate.start();
  TEST_ASSERT_FALSE(gate.feed(true, 9599999));
  TEST_ASSERT_TRUE(gate.feed(true, 1));
}

void test_monitor_voice_capture_gate() {
  speech::VoiceCaptureGate quiet;
  TEST_ASSERT_FALSE(quiet.feed(false, 95999));
  TEST_ASSERT_TRUE(quiet.feed(false, 1));
  TEST_ASSERT_FALSE(quiet.voiced());
  speech::VoiceCaptureGate resumed;
  TEST_ASSERT_FALSE(resumed.feed(true, 512));
  TEST_ASSERT_FALSE(resumed.feed(false, 15999));
  TEST_ASSERT_FALSE(resumed.feed(true, 512));
  TEST_ASSERT_FALSE(resumed.feed(false, 15999));
  TEST_ASSERT_TRUE(resumed.feed(false, 1));
  TEST_ASSERT_TRUE(resumed.voiced());
  speech::VoiceCaptureGate limit;
  TEST_ASSERT_FALSE(limit.feed(true, 143999));
  TEST_ASSERT_TRUE(limit.feed(true, 1));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_monitor_voice_capture_gate);
  RUN_TEST(test_recorder_ogg_boundaries_and_corruption);
  RUN_TEST(test_recorder_gate_silence_grace_reset_and_limit);
  RUN_TEST(test_sse_fragmented_status_comments_and_unknown_events);
  RUN_TEST(test_sse_never_delivers_incomplete_or_oversized_events);
  RUN_TEST(test_monitor_timeout_starts_after_active_work_finishes);
  RUN_TEST(test_monitor_touch_and_timeout_setting);
  RUN_TEST(test_monitor_timeout_across_millis_wrap);
  RUN_TEST(test_avatar_rgb565_channel_order);
  RUN_TEST(test_avatar_identity_and_state_are_independent);
  RUN_TEST(test_avatar_animation_changes_frame);
  RUN_TEST(test_native_runner_smoke);
  RUN_TEST(test_selects_matching_dns_sd_service_and_txt_path);
  RUN_TEST(test_endpoint_falls_back_only_when_configured);
  RUN_TEST(test_service_instance_match_is_exact_and_configured);
  RUN_TEST(test_wav_header_is_mono_16_bit_pcm);
  RUN_TEST(test_wav_header_matches_early_stopped_capture_length);
  RUN_TEST(test_multipart_uses_file_field_and_closing_boundary);
  return UNITY_END();
}
