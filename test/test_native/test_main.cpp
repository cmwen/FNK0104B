#include <unity.h>
#include <storage/bmp.hpp>
#include <image_codec/image.hpp>
#include <image_codec/allocator.h>
#include "../fixtures/photo-frame/encoded.hpp"

void assert_red_photo(const uint8_t* data, size_t size, uint8_t alpha = 255) {
  {
    image_codec::Image image;
    const char* error = nullptr;
    TEST_ASSERT_TRUE_MESSAGE(image_codec::decode(data, size, image, error), error ? error : "decode");
    TEST_ASSERT_EQUAL_INT(8, image.width);
    TEST_ASSERT_EQUAL_INT(6, image.height);
    TEST_ASSERT_NULL(error);
    TEST_ASSERT_UINT8_WITHIN(5, 255, image.rgba[0]);
    TEST_ASSERT_UINT8_WITHIN(5, 0, image.rgba[1]);
    TEST_ASSERT_UINT8_WITHIN(5, 0, image.rgba[2]);
    TEST_ASSERT_EQUAL_UINT8(alpha, image.rgba[3]);
  }
  TEST_ASSERT_EQUAL_UINT32(0, image_codec::allocatedBytes());
}

void test_photo_frame_real_decoders_and_cleanup() {
  using namespace photo_fixtures;
  for (int cycle = 0; cycle < 5; ++cycle) {
    assert_red_photo(jpeg, sizeof(jpeg));
    assert_red_photo(progressive, sizeof(progressive));
    assert_red_photo(png, sizeof(png));
    assert_red_photo(palette_png, sizeof(palette_png));
    assert_red_photo(webp_lossy, sizeof(webp_lossy));
    assert_red_photo(webp_lossless, sizeof(webp_lossless));
    assert_red_photo(alpha_png, sizeof(alpha_png), 128);
    assert_red_photo(alpha_webp, sizeof(alpha_webp), 128);
  }
  const uint8_t* large[] = {large_jpeg, large_png, large_webp};
  const size_t sizes[] = {sizeof(large_jpeg), sizeof(large_png), sizeof(large_webp)};
  for (unsigned i = 0; i < 3; ++i) {
    image_codec::Image image;
    const char* error = nullptr;
    TEST_ASSERT_TRUE_MESSAGE(image_codec::decode(large[i], sizes[i], image, error), error ? error : "large decode");
    TEST_ASSERT_EQUAL_INT(640, image.width);
    TEST_ASSERT_EQUAL_INT(480, image.height);
    auto fitted = image_codec::fit(image.width, image.height);
    TEST_ASSERT_EQUAL_INT(320, fitted.width);
    TEST_ASSERT_EQUAL_INT(240, fitted.height);
    image.clear();
    TEST_ASSERT_EQUAL_UINT32(0, image_codec::allocatedBytes());
  }
}

void* fail_image_allocate(size_t) { return nullptr; }
void* fail_image_reallocate(void*, size_t) { return nullptr; }
void fail_image_release(void*) {}

void test_photo_frame_rejects_invalid_and_exhausted_images() {
  using namespace photo_fixtures;
  image_codec::Image image;
  const char* error = nullptr;
  TEST_ASSERT_FALSE(image_codec::decode(animated_webp, sizeof(animated_webp), image, error));
  TEST_ASSERT_EQUAL_STRING("animated_webp_unsupported", error);
  TEST_ASSERT_FALSE(image_codec::decode(oversized_png, sizeof(oversized_png), image, error));
  TEST_ASSERT_FALSE(image_codec::decode(png, 16, image, error));
  TEST_ASSERT_FALSE(image_codec::decode(webp_lossless, sizeof(webp_lossless) - 12, image, error));
  TEST_ASSERT_FALSE(image_codec::decode(nullptr, 100, image, error));
  TEST_ASSERT_FALSE(image_codec::decode(jpeg, image_codec::kMaxEncodedBytes + 1, image, error));
  TEST_ASSERT_NULL(image.rgba);
  TEST_ASSERT_EQUAL_UINT32(0, image_codec::allocatedBytes());
  image_codec::setAllocator({fail_image_allocate, fail_image_reallocate, fail_image_release});
  TEST_ASSERT_FALSE(image_codec::decode(jpeg, sizeof(jpeg), image, error));
  TEST_ASSERT_FALSE(image_codec::decode(png, sizeof(png), image, error));
  TEST_ASSERT_FALSE(image_codec::decode(webp_lossless, sizeof(webp_lossless), image, error));
  TEST_ASSERT_EQUAL_UINT32(0, image_codec::allocatedBytes());
  image_codec::setAllocator({malloc, realloc, free});
  // Budget exhaustion must preserve a live allocation when realloc fails.
  void* allocation = frame_image_malloc(1024);
  TEST_ASSERT_NOT_NULL(allocation);
  TEST_ASSERT_NULL(frame_image_realloc(allocation, image_codec::kMaxWorkingBytes + 1));
  TEST_ASSERT_EQUAL_UINT32(1024, image_codec::allocatedBytes());
  frame_image_free(allocation);
  TEST_ASSERT_EQUAL_UINT32(0, image_codec::allocatedBytes());
}

void test_photo_frame_fit_and_alpha() {
  auto landscape = image_codec::fit(1024, 512);
  TEST_ASSERT_EQUAL_INT(320, landscape.width);
  TEST_ASSERT_EQUAL_INT(160, landscape.height);
  TEST_ASSERT_EQUAL_INT(40, landscape.y);
  auto portrait = image_codec::fit(512, 1024);
  TEST_ASSERT_EQUAL_INT(120, portrait.width);
  TEST_ASSERT_EQUAL_INT(240, portrait.height);
  TEST_ASSERT_EQUAL_INT(100, portrait.x);
  auto exact = image_codec::fit(320, 240);
  TEST_ASSERT_EQUAL_INT(0, exact.x);
  TEST_ASSERT_EQUAL_INT(0, exact.y);
  auto narrow = image_codec::fit(1, 1024);
  TEST_ASSERT_EQUAL_INT(1, narrow.width);
  TEST_ASSERT_EQUAL_INT(240, narrow.height);
  uint8_t red[] = {255, 0, 0, 255};
  TEST_ASSERT_EQUAL_HEX16(0xf800, image_codec::rgbaToRgb565(red));
  red[3] = 128;
  TEST_ASSERT_EQUAL_HEX16(0x8000, image_codec::rgbaToRgb565(red));
  red[3] = 0;
  TEST_ASSERT_EQUAL_HEX16(0, image_codec::rgbaToRgb565(red));
}

void test_photo_frame_bmp_validation_and_color() {
  uint8_t header[54] = {'B', 'M'};
  header[10] = 54; header[14] = 40;
  header[18] = 3; header[22] = 2;
  header[26] = 1; header[28] = 24;
  storage::BmpInfo info{};
  TEST_ASSERT_TRUE(storage::parseBmp(header, 54, 78, info));
  TEST_ASSERT_EQUAL_UINT32(12, info.stride);  // Three padding bytes per row.
  TEST_ASSERT_EQUAL_UINT32(66, info.rowOffset(0));
  TEST_ASSERT_EQUAL_UINT32(54, info.rowOffset(1));
  TEST_ASSERT_FALSE(storage::parseBmp(header, 54, 77, info));
  TEST_ASSERT_FALSE(storage::parseBmp(header, 53, 78, info));
  header[22] = 0xfe; header[23] = header[24] = header[25] = 0xff;
  TEST_ASSERT_TRUE(storage::parseBmp(header, 54, 78, info));
  TEST_ASSERT_EQUAL_UINT32(54, info.rowOffset(0));
  TEST_ASSERT_EQUAL_UINT32(66, info.rowOffset(1));
  header[30] = 1;
  TEST_ASSERT_FALSE(storage::parseBmp(header, 54, 78, info));
  header[30] = 0; header[28] = 32;
  TEST_ASSERT_FALSE(storage::parseBmp(header, 54, 78, info));
  header[28] = 24; header[18] = 0;
  TEST_ASSERT_FALSE(storage::parseBmp(header, 54, 78, info));
  header[18] = 0x41; header[19] = 1; // 321 pixels.
  TEST_ASSERT_FALSE(storage::parseBmp(header, 54, 2048, info));
  header[18] = 3; header[19] = 0; header[10] = 20;
  TEST_ASSERT_FALSE(storage::parseBmp(header, 54, 78, info));
  const uint8_t red[] = {0, 0, 255}, green[] = {0, 255, 0}, blue[] = {255, 0, 0};
  TEST_ASSERT_EQUAL_HEX16(0xf800, storage::bgrToRgb565(red));
  TEST_ASSERT_EQUAL_HEX16(0x07e0, storage::bgrToRgb565(green));
  TEST_ASSERT_EQUAL_HEX16(0x001f, storage::bgrToRgb565(blue));
}
#include "../../apps/codex-monitor/include/monitor_update_policy.hpp"
#include <codex_hid/protocol.hpp>
#include <codex_hid/backend.hpp>
#include <codex_hid/voice_controls.hpp>
#include <codex_hid/monitor_settings.hpp>
#include <ui/status_led.hpp>
#include <ui/micro_layout.hpp>
#include <ui/idle_photos.hpp>
#include <ui/idle_timer.hpp>

void test_monitor_idle_photos_keep_screen_timeout_and_validate_files() {
  ui::IdleTimer timer;
  timer.activity(0xfffffff0u);
  TEST_ASSERT_EQUAL_UINT32(32, timer.elapsed(16));
  ui::photos::Settings settings;
  const uint8_t valid[] = {1, 2, 5, 0, 10, 0};
  TEST_ASSERT_TRUE(ui::photos::decode(valid, 6, settings));
  TEST_ASSERT_FALSE(ui::photos::idle(settings, 4999, false, true));
  TEST_ASSERT_TRUE(ui::photos::idle(settings, 5000, false, true));
  TEST_ASSERT_FALSE(ui::photos::idle(settings, 6000, true, true));
  TEST_ASSERT_FALSE(ui::photos::idle(settings, 6000, false, false));
  TEST_ASSERT_FALSE(timer.expired(1000, 60000, true));
  TEST_ASSERT_TRUE(ui::photos::idle(settings, timer.elapsed(6000), false, true));
  TEST_ASSERT_TRUE(timer.expired(61000, 60000, false));
  uint8_t invalid[] = {1, 3, 5, 0, 10, 0};
  TEST_ASSERT_FALSE(ui::photos::decode(invalid, 6, settings));
  invalid[1] = 2; invalid[2] = 4;
  TEST_ASSERT_FALSE(ui::photos::decode(invalid, 6, settings));
  TEST_ASSERT_TRUE(ui::photos::validName("lake-1.fpf"));
  TEST_ASSERT_FALSE(ui::photos::validName("../lake.fpf"));
  TEST_ASSERT_FALSE(ui::photos::validName(".fpf"));
  TEST_ASSERT_FALSE(ui::photos::validName("lake.jpg"));
  const uint8_t known[] = {'1','2','3','4','5','6','7','8','9'};
  TEST_ASSERT_EQUAL_HEX32(0xcbf43926, ui::photos::crcUpdate(~0u, known, sizeof(known)) ^ ~0u);
  uint8_t header[16] = {'F','P','F','1',64,1,196,0,0,0xea,1,0};
  // 320*196*2 = 125440 = 0x1ea00.
  TEST_ASSERT_TRUE(ui::photos::validHeader(header, ui::photos::fileBytes));
  TEST_ASSERT_FALSE(ui::photos::validHeader(header, ui::photos::fileBytes - 1));
  header[6] = 240;
  TEST_ASSERT_FALSE(ui::photos::validHeader(header, ui::photos::fileBytes));
}
#include <initializer_list>

#include <string.h>

#include <locallink/protocol.hpp>
#include <locallink/sse.hpp>
#include <ui/avatar_assets.hpp>
#include <ui/micro_avatar.hpp>
#include <ui/idle_timer.hpp>
#include <ui/monitor_theme.hpp>
#include <recorder/ogg.hpp>
#include <keypad/layout.hpp>
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

void test_micro_avatar_status_and_refresh() {
  codex_hid::Protocol protocol;
  char reply[1024];
  auto update = [&](const char* json) {
    TEST_ASSERT_TRUE(protocol.process(json, strlen(json), reply, sizeof(reply)));
  };
  update(R"({"m":"v.oai.thstatus","p":[{"id":0,"c":3166206,"b":1},{"id":1,"c":16777215},{"id":2,"c":16739584},{"id":3,"c":16711731}]})");
  TEST_ASSERT_EQUAL(int(ui::avatar::Mood::Thinking), int(ui::micro::mood(protocol.status.slots[0])));
  TEST_ASSERT_EQUAL(int(ui::avatar::Mood::Idle), int(ui::micro::mood(protocol.status.slots[1])));
  TEST_ASSERT_EQUAL(int(ui::avatar::Mood::NeedsInput), int(ui::micro::mood(protocol.status.slots[2])));
  TEST_ASSERT_EQUAL(int(ui::avatar::Mood::Error), int(ui::micro::mood(protocol.status.slots[3])));
  update(R"({"m":"v.oai.thstatus","p":[{"id":0,"b":0},{"id":1,"e":4,"s":0.4}]})");
  TEST_ASSERT_EQUAL(int(ui::avatar::Mood::Thinking), int(ui::micro::mood(protocol.status.slots[0])));
  TEST_ASSERT_NOT_EQUAL(ui::monitor::kTrack, ui::micro::statusColor(protocol.status.slots[0]));
  TEST_ASSERT_NOT_EQUAL(0, ui::micro::statusColor(protocol.status.slots[0]));
  TEST_ASSERT_EQUAL(int(ui::avatar::Mood::Idle), int(ui::micro::mood(protocol.status.slots[1])));
  update(R"({"m":"v.oai.thstatus","p":[{"id":0,"c":0}]})");
  TEST_ASSERT_EQUAL(ui::monitor::kTrack, ui::micro::statusColor(protocol.status.slots[0]));
  TEST_ASSERT_EQUAL(int(ui::avatar::Mood::Idle), int(ui::micro::mood(protocol.status.slots[0])));
  ui::avatar::Canvas canvas;
  ui::avatar::render(canvas, 42, ui::avatar::Mood::Thinking, 12, ui::monitor::kPanel, false);
  TEST_ASSERT_EQUAL(ui::monitor::kPanel, canvas.pixels[2 * 32 + 2]);
  TEST_ASSERT_EQUAL(ui::monitor::kPanel, canvas.pixels[0]);
}

void test_micro_agent_display_retains_state_across_lighting_off_and_silence() {
  codex_hid::Protocol protocol;
  codex_hid::Status received, displayed;
  char reply[1024];
  auto update = [&](const char* json) {
    TEST_ASSERT_TRUE(protocol.process(json, strlen(json), reply, sizeof(reply)));
    TEST_ASSERT_TRUE(codex_hid::applyStatusSnapshot(protocol.status, received));
    ui::micro::updateAgentDisplay(received, displayed);
  };
  update(R"({"m":"v.oai.thstatus","p":[{"id":0,"c":3166206},{"id":1,"c":16777215},{"id":2,"c":16739584},{"id":3,"c":16711731}]})");
  const auto working = ui::micro::statusColor(displayed.slots[0]);
  update(R"({"m":"v.oai.thstatus","p":[{"id":0,"c":0,"b":0},{"id":1,"c":0},{"id":2,"c":0},{"id":3,"c":0}]})");
  TEST_ASSERT_EQUAL_UINT32(0, received.slots[0].color);
  TEST_ASSERT_EQUAL(int(ui::avatar::Mood::Thinking), int(ui::micro::mood(displayed.slots[0])));
  TEST_ASSERT_EQUAL(working, ui::micro::statusColor(displayed.slots[0]));
  TEST_ASSERT_EQUAL_HEX16(0xffff, ui::micro::statusColor(displayed.slots[1]));
  TEST_ASSERT_EQUAL(int(ui::avatar::Mood::NeedsInput), int(ui::micro::mood(displayed.slots[2])));
  TEST_ASSERT_EQUAL(int(ui::avatar::Mood::Error), int(ui::micro::mood(displayed.slots[3])));
  // Quiet polling/repainting must not replay the raw black snapshot over the cache.
  for (unsigned i = 0; i < 10; ++i) {
    TEST_ASSERT_FALSE(codex_hid::applyStatusSnapshot(protocol.status, received));
    TEST_ASSERT_EQUAL(working, ui::micro::statusColor(displayed.slots[0]));
  }
  update(R"({"m":"v.oai.thstatus","p":[{"id":0,"c":16777215,"b":0}]})");
  TEST_ASSERT_EQUAL(int(ui::avatar::Mood::Idle), int(ui::micro::mood(displayed.slots[0])));
  TEST_ASSERT_EQUAL_HEX16(0xffff, ui::micro::statusColor(displayed.slots[0]));
  TEST_ASSERT_EQUAL(int(ui::avatar::Mood::Error), int(ui::micro::mood(displayed.slots[3])));
  TEST_ASSERT_FALSE(displayed.slots[5].present);
  update(R"({"m":"v.oai.thstatus","p":[]})");
  TEST_ASSERT_EQUAL_HEX16(0xffff, ui::micro::statusColor(displayed.slots[0]));
  // Real connection invalidation still clears values belonging to the old host.
  ui::micro::updateAgentDisplay(codex_hid::Status{}, displayed);
  TEST_ASSERT_FALSE(displayed.slots[0].present);
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
  TEST_ASSERT_FALSE(quiet.feed(false, 159999));
  TEST_ASSERT_TRUE(quiet.feed(false, 1));
  TEST_ASSERT_FALSE(quiet.voiced());
  speech::VoiceCaptureGate resumed;
  TEST_ASSERT_FALSE(resumed.feed(true, 512));
  TEST_ASSERT_FALSE(resumed.feed(false, 63999));
  TEST_ASSERT_FALSE(resumed.feed(true, 512));
  TEST_ASSERT_FALSE(resumed.feed(false, 63999));
  TEST_ASSERT_TRUE(resumed.feed(false, 1));
  TEST_ASSERT_TRUE(resumed.voiced());
  speech::VoiceCaptureGate limit;
  TEST_ASSERT_FALSE(limit.feed(true, 479999));
  TEST_ASSERT_TRUE(limit.feed(true, 1));
}

void test_monitor_reset_countdown() {
  using ui::monitor::resetPixels;
  TEST_ASSERT_EQUAL(-1, resetPixels(-1, 100, 18000, 16));
  TEST_ASSERT_EQUAL(-1, resetPixels(18100, -1, 18000, 16));
  TEST_ASSERT_EQUAL(16, resetPixels(18100, 100, 18000, 16));
  TEST_ASSERT_EQUAL(8, resetPixels(9100, 100, 18000, 16));
  TEST_ASSERT_EQUAL(0, resetPixels(100, 100, 18000, 16));
  TEST_ASSERT_EQUAL(0, resetPixels(99, 100, 18000, 16));
  TEST_ASSERT_EQUAL(16, resetPixels(20000, 100, 18000, 16));
  TEST_ASSERT_EQUAL(12, resetPixels(302500, 100, 604800, 24));
}

void test_monitor_microphone_level() {
  const int16_t quiet[] = {0, 0, 0, 0};
  const int16_t low[] = {128, -128, 128, -128};
  const int16_t loud[] = {4096, -4096, 4096, -4096};
  const int16_t clipped[] = {-32768, 32767};
  TEST_ASSERT_EQUAL(0, speech::microphoneLevel(quiet, 4));
  TEST_ASSERT_GREATER_THAN(0, speech::microphoneLevel(low, 4));
  TEST_ASSERT_GREATER_THAN(speech::microphoneLevel(low, 4), speech::microphoneLevel(loud, 4));
  TEST_ASSERT_LESS_OR_EQUAL(100, speech::microphoneLevel(clipped, 2));
  TEST_ASSERT_EQUAL(0, speech::microphoneLevel(nullptr, 0));
}

void test_keypad_touch_boundaries_and_gaps() {
  TEST_ASSERT_EQUAL(0, keypad::cell(8, 58, 58, 31, 5));
  TEST_ASSERT_EQUAL(19, keypad::cell(305, 206, 58, 31, 5));
  TEST_ASSERT_EQUAL(-1, keypad::cell(7, 58, 58, 31, 5));
  TEST_ASSERT_EQUAL(-1, keypad::cell(312, 58, 58, 31, 5));
  TEST_ASSERT_EQUAL(-1, keypad::cell(8, 57, 58, 31, 5));
  TEST_ASSERT_EQUAL(-1, keypad::cell(8, 213, 58, 31, 5));
  TEST_ASSERT_EQUAL(-1, keypad::cell(78, 58, 58, 31, 5));
  TEST_ASSERT_EQUAL(-1, keypad::cell(8, 84, 58, 31, 5));
  TEST_ASSERT_EQUAL(11, keypad::cell(305, 169, 58, 39, 3));
  TEST_ASSERT_EQUAL(-1, keypad::cell(8, 175, 58, 39, 3));
}

void test_codex_hid_descriptor_contract() {
  unsigned size = 0, count = 0, id = 0, page = 0, inputBits = 0, outputBits = 0;
  const auto* bytes = codex_hid::kDescriptor;
  for (size_t at = 0; at < sizeof(codex_hid::kDescriptor); ) {
    unsigned prefix = bytes[at++]; unsigned length = prefix & 3;
    if (length == 3) length = 4;
    TEST_ASSERT_TRUE(at + length <= sizeof(codex_hid::kDescriptor));
    unsigned value = 0;
    for (unsigned i = 0; i < length; ++i) value |= unsigned(bytes[at++]) << (i * 8);
    switch (prefix & 0xfc) {
      case 0x04: page = value; break;
      case 0x74: size = value; break;
      case 0x94: count = value; break;
      case 0x84: id = value; break;
      case 0x80: inputBits += size * count; break;
      case 0x90: outputBits += size * count; break;
    }
  }
  TEST_ASSERT_EQUAL_HEX16(0xff00, page); TEST_ASSERT_EQUAL(6, id);
  TEST_ASSERT_EQUAL(63 * 8, inputBits); TEST_ASSERT_EQUAL(63 * 8, outputBits);
}

void test_codex_hid_framing_bounds_and_recovery() {
  codex_hid::Decoder decoder;
  struct Capture { unsigned count = 0; char json[256]{}; } capture;
  auto handler = [](const char* text, size_t n, void* context) {
    auto& c = *static_cast<Capture*>(context); ++c.count;
    if (n < sizeof(c.json)) { memcpy(c.json, text, n); c.json[n] = 0; }
  };
  const char* json = "{\"m\":\"device.status\",\"p\":{\"text\":\"}\\\"{\"},\"id\":7}";
  uint8_t body[63]; size_t offset = 0;
  while (size_t n = codex_hid::frame(json, strlen(json), offset, body)) {
    TEST_ASSERT_TRUE(decoder.feed(body, sizeof(body), handler, &capture)); offset += n;
  }
  TEST_ASSERT_EQUAL(1, capture.count); TEST_ASSERT_EQUAL_STRING(json, capture.json);
  body[0] = 2; body[1] = 62;
  TEST_ASSERT_FALSE(decoder.feed(body, sizeof(body), handler, &capture));
  TEST_ASSERT_FALSE(decoder.feed(body, 1, handler, &capture));
  TEST_ASSERT_FALSE(decoder.feed(nullptr, 63, handler, &capture));
  const char* newlineFree = "{\"m\":\"sys.version\"}";
  codex_hid::frame(newlineFree, strlen(newlineFree), 0, body);
  body[1] = strlen(newlineFree);
  TEST_ASSERT_TRUE(decoder.feed(body, 2 + strlen(newlineFree), handler, &capture));
  TEST_ASSERT_EQUAL(2, capture.count);
  const char* longJson = "{\"m\":\"device.status\",\"p\":{\"text\":\"abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz\"},\"id\":9}";
  offset = 0;
  while (size_t n = codex_hid::frame(longJson, strlen(longJson), offset, body)) {
    TEST_ASSERT_TRUE(decoder.feed(body, 63, handler, &capture)); offset += n;
  }
  TEST_ASSERT_EQUAL_STRING(longJson, capture.json);
  TEST_ASSERT_EQUAL(3, capture.count);
  const char* two = "{\"m\":\"sys.version\"}{\"m\":\"device.status\"}";
  codex_hid::frame(two, strlen(two), 0, body);
  decoder.feed(body, 63, handler, &capture); TEST_ASSERT_EQUAL(5, capture.count);
  // Overflow discards the whole object, then resynchronizes at its closing brace.
  uint8_t large[63]{}; large[0] = 2; large[1] = 61; memset(large + 2, ' ', 61); large[2] = '{';
  decoder.feed(large, 63, handler, &capture); large[2] = ' ';
  for (unsigned i = 0; i < 40; ++i) decoder.feed(large, 63, handler, &capture);
  large[1] = 1; large[2] = '}'; decoder.feed(large, 63, handler, &capture);
  codex_hid::frame(newlineFree, strlen(newlineFree), 0, body);
  decoder.feed(body, 63, handler, &capture); TEST_ASSERT_EQUAL(6, capture.count);
}

void test_codex_hid_discovery_status_events_and_unknown_calls() {
  codex_hid::Protocol protocol; char output[1024];
  auto call = [&](const char* input) { return protocol.process(input, strlen(input), output, sizeof(output)); };
  TEST_ASSERT_TRUE(call("{\"m\":\"device.status\",\"id\":8}"));
  StaticJsonDocument<1024> response; TEST_ASSERT_FALSE(deserializeJson(response, output));
  TEST_ASSERT_EQUAL(8, response["id"].as<int>());
  TEST_ASSERT_EQUAL_STRING("0.1.0-fnk0104b-hid", response["result"]["version"]);
  TEST_ASSERT_TRUE(call("{\"method\":\"sys.version\",\"id\":\"abc\"}"));
  deserializeJson(response, output); TEST_ASSERT_EQUAL_STRING("abc", response["id"]);
  TEST_ASSERT_TRUE(call("{\"m\":\"v.oai.thstatus\",\"p\":[{\"id\":0,\"c\":123,\"b\":0.5,\"e\":4},{\"id\":99}]}"));
  TEST_ASSERT_EQUAL(1, protocol.status.revision);
  TEST_ASSERT_TRUE(protocol.status.slots[0].present); TEST_ASSERT_EQUAL(123, protocol.status.slots[0].color);
  TEST_ASSERT_EQUAL_STRING("4", protocol.status.slots[0].effect);
  TEST_ASSERT_EQUAL(0, call("{\"m\":\"v.oai.thstatus\",\"p\":null}"));
  TEST_ASSERT_EQUAL(1, protocol.status.revision);
  TEST_ASSERT_TRUE(call("{\"m\":\"unknown\",\"id\":1}"));
  deserializeJson(response, output); TEST_ASSERT_EQUAL(-32601, response["error"]["code"].as<int>());
  TEST_ASSERT_EQUAL(0, call("{\"m\":\"unknown\"}"));
  TEST_ASSERT_EQUAL(0, call("{bad json}"));
  TEST_ASSERT_TRUE(codex_hid::agentEvent(true, output, sizeof(output)));
  deserializeJson(response, output); TEST_ASSERT_EQUAL(1, response["p"]["act"].as<int>());
  TEST_ASSERT_TRUE(codex_hid::agentEvent(false, output, sizeof(output)));
  deserializeJson(response, output); TEST_ASSERT_EQUAL(0, response["p"]["act"].as<int>());
  TEST_ASSERT_TRUE(codex_hid::microphoneEvent(true, output, sizeof(output)));
  deserializeJson(response, output);
  TEST_ASSERT_EQUAL_STRING("v.oai.hid", response["m"]);
  TEST_ASSERT_EQUAL_STRING("ACT10", response["p"]["k"]);
  TEST_ASSERT_EQUAL(1, response["p"]["act"].as<int>());
  TEST_ASSERT_TRUE(codex_hid::microphoneEvent(false, output, sizeof(output)));
  deserializeJson(response, output); TEST_ASSERT_EQUAL(0, response["p"]["act"].as<int>());
}

void test_usb_loss_preserves_connection_and_last_status() {
  codex_hid::ReceiveEpochs epochs;
  epochs.reconnect();
  const auto connection = epochs.connection(), fragments = epochs.fragments();
  codex_hid::Protocol protocol;
  char reply[1024];
  const char* status = "{\"m\":\"v.oai.thstatus\",\"p\":[{\"id\":0,\"c\":123},{\"id\":1,\"c\":456},{\"id\":2,\"c\":789}]}";
  protocol.process(status, strlen(status), reply, sizeof(reply));
  codex_hid::Decoder decoder;
  auto handler = [](const char* json, size_t size, void* context) {
    char output[1024];
    static_cast<codex_hid::Protocol*>(context)->process(json, size, output, sizeof(output));
  };
  uint8_t body[codex_hid::kBodySize];
  // Begin a long status update, lose its continuation, then receive a new update.
  codex_hid::frame(status, strlen(status), 0, body);
  TEST_ASSERT_TRUE(decoder.feed(body, sizeof(body), handler, &protocol));
  // A lost report resets framing without entering the connection-reset branch.
  epochs.overflow(); epochs.overflow();
  TEST_ASSERT_EQUAL(connection, epochs.connection());
  TEST_ASSERT_NOT_EQUAL(fragments, epochs.fragments());
  decoder.reset();
  for (unsigned i = 0; i < 3; ++i) TEST_ASSERT_TRUE(protocol.status.slots[i].present);
  TEST_ASSERT_EQUAL(456, protocol.status.slots[1].color);
  const char* update = "{\"m\":\"v.oai.thstatus\",\"p\":[{\"id\":1,\"c\":999}]}";
  codex_hid::frame(update, strlen(update), 0, body);
  TEST_ASSERT_TRUE(decoder.feed(body, sizeof(body), handler, &protocol));
  TEST_ASSERT_EQUAL(2, protocol.status.revision);
  TEST_ASSERT_EQUAL(123, protocol.status.slots[0].color);
  TEST_ASSERT_EQUAL(999, protocol.status.slots[1].color);
  TEST_ASSERT_EQUAL(789, protocol.status.slots[2].color);
  epochs.reconnect();
  TEST_ASSERT_NOT_EQUAL(connection, epochs.connection());
}

void test_micro_status_mailbox_survives_discovery_and_repaint() {
  codex_hid::Protocol protocol;
  codex_hid::Status displayed;
  char reply[1024];
  auto call = [&](const char* input) {
    return protocol.process(input, strlen(input), reply, sizeof(reply));
  };
  // Desktop sends lighting before its discovery reply has finished transmitting.
  TEST_ASSERT_TRUE(call("{\"m\":\"v.oai.thstatus\",\"p\":[{\"id\":0,\"c\":3166206},{\"id\":1,\"c\":16739584}]}"));
  TEST_ASSERT_TRUE(codex_hid::applyStatusSnapshot(protocol.status, displayed));
  // Selecting Micro mode clears the UI snapshot; the retained mailbox restores it.
  displayed = codex_hid::Status{};
  TEST_ASSERT_TRUE(codex_hid::applyStatusSnapshot(protocol.status, displayed));
  TEST_ASSERT_EQUAL_HEX32(0x304ffe, displayed.slots[0].color);
  TEST_ASSERT_EQUAL_HEX32(0xff6d00, displayed.slots[1].color);
  for (unsigned repaint = 0; repaint < 3; ++repaint) {
    TEST_ASSERT_FALSE(codex_hid::applyStatusSnapshot(protocol.status, displayed));
    TEST_ASSERT_TRUE(ui::micro::mood(displayed.slots[0]) == ui::avatar::Mood::Thinking);
  }
  // Discovery calls and empty/partial status packets do not erase other slots.
  TEST_ASSERT_TRUE(call("{\"m\":\"device.status\",\"id\":1}"));
  TEST_ASSERT_FALSE(codex_hid::applyStatusSnapshot(protocol.status, displayed));
  TEST_ASSERT_TRUE(call("{\"m\":\"v.oai.thstatus\",\"p\":[]}"));
  TEST_ASSERT_TRUE(codex_hid::applyStatusSnapshot(protocol.status, displayed));
  TEST_ASSERT_EQUAL_HEX32(0x304ffe, displayed.slots[0].color);
  TEST_ASSERT_TRUE(call("{\"m\":\"v.oai.thstatus\",\"p\":[{\"id\":1,\"c\":0}]}"));
  TEST_ASSERT_TRUE(codex_hid::applyStatusSnapshot(protocol.status, displayed));
  TEST_ASSERT_EQUAL_HEX32(0x304ffe, displayed.slots[0].color);
  TEST_ASSERT_EQUAL(0, displayed.slots[1].color);
  // A real connection reset still invalidates status.
  protocol.status = codex_hid::Status{};
  TEST_ASSERT_TRUE(codex_hid::applyStatusSnapshot(protocol.status, displayed));
  TEST_ASSERT_FALSE(displayed.slots[0].present);
  // Revisions may restart on reconnect; equal revisions cannot hide new values.
  displayed.revision = 1;
  TEST_ASSERT_TRUE(call("{\"m\":\"v.oai.thstatus\",\"p\":[{\"id\":0,\"c\":16711731}]}"));
  TEST_ASSERT_TRUE(codex_hid::applyStatusSnapshot(protocol.status, displayed));
  TEST_ASSERT_TRUE(ui::micro::mood(displayed.slots[0]) == ui::avatar::Mood::Error);
}

void test_monitor_micro_mode_requires_desktop_discovery() {
  TEST_ASSERT_FALSE(codex_hid::microConnected(codex_hid::LinkState::Off));
  TEST_ASSERT_FALSE(codex_hid::microConnected(codex_hid::LinkState::Usb));
  TEST_ASSERT_TRUE(codex_hid::microConnected(codex_hid::LinkState::Linked));
  // A quiet Desktop connection stays usable until USB disconnects.
  TEST_ASSERT_TRUE(codex_hid::microConnected(codex_hid::LinkState::Idle));
  using codex_hid::LinkState; using codex_hid::Transport;
  TEST_ASSERT_TRUE(codex_hid::chooseTransport(LinkState::Usb, LinkState::Off) == Transport::None);
  TEST_ASSERT_TRUE(codex_hid::chooseTransport(LinkState::Usb, LinkState::Linked) == Transport::Ble);
  TEST_ASSERT_TRUE(codex_hid::chooseTransport(LinkState::Linked, LinkState::Linked) == Transport::Usb);
  TEST_ASSERT_TRUE(codex_hid::chooseTransport(LinkState::Idle, LinkState::Linked) == Transport::Usb);
  TEST_ASSERT_TRUE(codex_hid::chooseTransport(LinkState::Off, LinkState::Idle) == Transport::Ble);
}

void test_micro_six_keys_and_touch_gaps() {
  TEST_ASSERT_EQUAL(10, ui::micro::bottomAt(4, 191));
  TEST_ASSERT_EQUAL(10, ui::micro::bottomAt(115, 236));
  TEST_ASSERT_EQUAL(-1, ui::micro::bottomAt(116, 200));
  TEST_ASSERT_EQUAL(11, ui::micro::bottomAt(122, 191));
  TEST_ASSERT_EQUAL(11, ui::micro::bottomAt(203, 236));
  TEST_ASSERT_EQUAL(-1, ui::micro::bottomAt(204, 200));
  TEST_ASSERT_EQUAL(12, ui::micro::bottomAt(210, 191));
  TEST_ASSERT_EQUAL(12, ui::micro::bottomAt(315, 236));
  TEST_ASSERT_EQUAL(-1, ui::micro::bottomAt(10, 237));
  char json[128]; StaticJsonDocument<256> event;
  for (unsigned key = 0; key <= 12; ++key) {
    for (bool pressed : {false, true}) {
      TEST_ASSERT_TRUE(codex_hid::keyEvent(key, pressed, json, sizeof(json)));
      TEST_ASSERT_FALSE(deserializeJson(event, json));
      char expected[8]; snprintf(expected, sizeof(expected), key < 6 ? "AG%02u" : "ACT%02u", key);
      TEST_ASSERT_EQUAL_STRING(expected, event["p"]["k"]);
      TEST_ASSERT_EQUAL(pressed ? 1 : 0, event["p"]["act"].as<int>());
      if (key < 6) TEST_ASSERT_EQUAL(key, event["p"]["ag"].as<unsigned>());
      else TEST_ASSERT_FALSE(event["p"].containsKey("ag"));
    }
  }
  const float angles[] = {.75f, .25f, 0.f, .5f};
  for (unsigned n = 0; n < 4; ++n) {
    for (bool pressed : {true, false}) {
      TEST_ASSERT_TRUE(codex_hid::keyEvent(13 + n, pressed, json, sizeof(json)));
      TEST_ASSERT_FALSE(deserializeJson(event, json));
      TEST_ASSERT_EQUAL_STRING("v.oai.rad", event["m"]);
      TEST_ASSERT_FLOAT_WITHIN(.001f, pressed ? angles[n] : 0.f, event["p"]["a"].as<float>());
      TEST_ASSERT_EQUAL(pressed ? 1 : 0, event["p"]["d"].as<int>());
    }
    const int x = ui::micro::directionX(n);
    TEST_ASSERT_EQUAL(13 + n, ui::micro::directionAt(x, 123));
    TEST_ASSERT_EQUAL(13 + n, ui::micro::directionAt(x + 73, 182));
    TEST_ASSERT_EQUAL(-1, ui::micro::directionAt(x + 74, 123));
    TEST_ASSERT_EQUAL(-1, ui::micro::directionAt(x, 183));
  }
  codex_hid::Slot a, b;
  TEST_ASSERT_TRUE(codex_hid::sameAppearance(a, b));
  b.speed = 1; strcpy(b.effect, "breath");
  TEST_ASSERT_TRUE(codex_hid::sameAppearance(a, b));
  b.color = 0xff00;
  TEST_ASSERT_FALSE(codex_hid::sameAppearance(a, b));
  TEST_ASSERT_EQUAL(0, codex_hid::keyEvent(17, true, json, sizeof(json)));
  TEST_ASSERT_EQUAL(0, codex_hid::keyEvent(0, true, json, 8));
  for (unsigned slot = 0; slot < 6; ++slot) {
    const int x = ui::micro::tileX(slot), y = ui::micro::tileY(slot);
    TEST_ASSERT_EQUAL(slot, ui::micro::tileAt(x, y));
    TEST_ASSERT_EQUAL(slot, ui::micro::tileAt(x + 99, y + 59));
    TEST_ASSERT_EQUAL(-1, ui::micro::tileAt(x + 100, y));
    TEST_ASSERT_EQUAL(-1, ui::micro::tileAt(x, y + 60));
    TEST_ASSERT_LESS_THAN(191, y + 60);
    TEST_ASSERT_LESS_THAN(317, x + 100);
  }
  TEST_ASSERT_EQUAL(-1, ui::micro::tileAt(80, 40));
  TEST_ASSERT_EQUAL(-1, ui::micro::tileAt(120, 200));
}

void test_usb_voice_default_hold_and_separate_toggle() {
  codex_hid::VoiceControls controls;
  TEST_ASSERT_FALSE(controls.audioOpen());
  TEST_ASSERT_FALSE(controls.pressVoice(0));
  TEST_ASSERT_TRUE(controls.pressPtt());
  TEST_ASSERT_TRUE(controls.audioOpen());
  controls.releasePtt();
  TEST_ASSERT_FALSE(controls.audioOpen());
  controls.configure(true);
  TEST_ASSERT_TRUE(controls.pressVoice(100));
  TEST_ASSERT_FALSE(controls.audioOpen()); // Holding to end must not reopen audio.
  controls.releaseVoice(200);
  TEST_ASSERT_TRUE(controls.audioOpen());
  TEST_ASSERT_FALSE(controls.pressPtt()); // No overlapping dictation and voice.
  TEST_ASSERT_TRUE(controls.pressVoice(300));
  TEST_ASSERT_FALSE(controls.audioOpen());
  controls.releaseVoice(400);
  TEST_ASSERT_TRUE(controls.pressPtt());
  TEST_ASSERT_FALSE(controls.pressVoice(500));
  controls.releasePtt();
  TEST_ASSERT_TRUE(controls.pressVoice(600));
  TEST_ASSERT_FALSE(controls.audioOpen());
  TEST_ASSERT_FALSE(controls.tick(1599));
  TEST_ASSERT_TRUE(controls.tick(1600));
  TEST_ASSERT_FALSE(controls.audioOpen());
  controls.releaseVoice(1600);
  TEST_ASSERT_TRUE(controls.pressVoice(UINT32_MAX - 500));
  TEST_ASSERT_TRUE(controls.tick(499)); // millis wrap.
  TEST_ASSERT_FALSE(controls.audioOpen());
  controls.releaseVoice(600);
  TEST_ASSERT_FALSE(controls.audioOpen());
  TEST_ASSERT_TRUE(controls.pressVoice(1000));
  controls.reset(); // Disconnect or failed HID request closes all audio.
  TEST_ASSERT_FALSE(controls.audioOpen());
  TEST_ASSERT_TRUE(controls.voiceEnabled());
  controls.configure(false);
  TEST_ASSERT_FALSE(controls.pressVoice(1100));
  char output[256]; StaticJsonDocument<256> event;
  TEST_ASSERT_TRUE(codex_hid::keyEvent(11, true, output, sizeof(output)));
  TEST_ASSERT_FALSE(deserializeJson(event, output));
  TEST_ASSERT_EQUAL_STRING("ACT11", event["p"]["k"]);
  TEST_ASSERT_EQUAL(1, event["p"]["act"].as<int>());
  TEST_ASSERT_TRUE(codex_hid::keyEvent(11, false, output, sizeof(output)));
  deserializeJson(event, output);
  TEST_ASSERT_EQUAL(0, event["p"]["act"].as<int>());
}

void test_monitor_settings_v3_validation_and_legacy_preservation() {
  codex_hid::MonitorSettings settings;
  TEST_ASSERT_FALSE(settings.separateVoice);
  const uint8_t enabled[]{3, 60, 120, 0, 6, 1};
  TEST_ASSERT_TRUE(codex_hid::decodeMonitorSettings(enabled, sizeof(enabled), settings));
  TEST_ASSERT_TRUE(settings.separateVoice);
  TEST_ASSERT_EQUAL(120, settings.timeout);
  const uint8_t legacy[]{1, 25, 5, 0};
  TEST_ASSERT_TRUE(codex_hid::decodeMonitorSettings(legacy, sizeof(legacy), settings));
  TEST_ASSERT_TRUE(settings.separateVoice);
  TEST_ASSERT_EQUAL(6, settings.slots);
  const uint8_t v2[]{2, 50, 30, 0, 3};
  TEST_ASSERT_TRUE(codex_hid::decodeMonitorSettings(v2, sizeof(v2), settings));
  TEST_ASSERT_TRUE(settings.separateVoice);
  TEST_ASSERT_EQUAL(3, settings.slots);
  const uint8_t invalid[]{3, 70, 5, 0, 6, 2};
  TEST_ASSERT_FALSE(codex_hid::decodeMonitorSettings(invalid, sizeof(invalid), settings));
  TEST_ASSERT_EQUAL(50, settings.volume); // Invalid writes change nothing.
  TEST_ASSERT_FALSE(codex_hid::decodeMonitorSettings(enabled, 5, settings));
  TEST_ASSERT_FALSE(codex_hid::decodeMonitorSettings(nullptr, 6, settings));
  const uint8_t disabled[]{3, 50, 30, 0, 3, 0};
  TEST_ASSERT_TRUE(codex_hid::decodeMonitorSettings(disabled, sizeof(disabled), settings));
  TEST_ASSERT_FALSE(settings.separateVoice);
  TEST_ASSERT_FALSE(settings.avatars);
  const uint8_t avatar[]{4, 50, 30, 0, 3, 0, 1};
  TEST_ASSERT_TRUE(codex_hid::decodeMonitorSettings(avatar, sizeof(avatar), settings));
  TEST_ASSERT_TRUE(settings.avatars);
  TEST_ASSERT_TRUE(codex_hid::decodeMonitorSettings(enabled, sizeof(enabled), settings));
  TEST_ASSERT_TRUE(settings.avatars); // Old clients preserve the newer preference.
  const uint8_t badAvatar[]{4, 50, 30, 0, 3, 0, 2};
  TEST_ASSERT_FALSE(codex_hid::decodeMonitorSettings(badAvatar, sizeof(badAvatar), settings));
  TEST_ASSERT_TRUE(settings.avatars);
  TEST_ASSERT_TRUE(settings.separateVoice);
}

void test_status_led_timeout_priority_and_settings() {
  using namespace ui::led;
  Indicator led;
  TEST_ASSERT_EQUAL(int(State::Off), int(led.update(State::Off, 0, true, true, 60, 0)));
  TEST_ASSERT_EQUAL(int(State::Working), int(led.update(State::Working, 1, true, true, 60, 100)));
  TEST_ASSERT_EQUAL(int(State::Working), int(led.update(State::Working, 1, true, true, 60, 60099)));
  TEST_ASSERT_EQUAL(int(State::Off), int(led.update(State::Working, 1, true, true, 60, 60100)));
  TEST_ASSERT_EQUAL(int(State::Working), int(led.update(State::Working, 2, true, true, 60, 61000)));
  TEST_ASSERT_EQUAL(int(State::Attention), int(led.update(State::Attention, 2, true, true, 60, 62000)));
  TEST_ASSERT_EQUAL(int(State::Off), int(led.update(State::Off, 0, false, true, 60, 63000)));
  TEST_ASSERT_EQUAL(int(State::Off), int(led.update(State::Off, 0, true, true, 60, 64000)));
  led.update(State::Working, 1, true, true, 60, 65000);
  TEST_ASSERT_EQUAL(int(State::Complete), int(led.update(State::Off, 0, true, true, 60, 66000)));
  TEST_ASSERT_EQUAL(int(State::Complete), int(led.update(State::Off, 0, true, true, 60, 125999)));
  TEST_ASSERT_EQUAL(int(State::Off), int(led.update(State::Off, 0, true, true, 60, 126000)));
  TEST_ASSERT_EQUAL(int(State::Off), int(led.update(State::Working, 1, true, false, 60, 127000)));
  TEST_ASSERT_EQUAL(int(State::Off), int(led.update(State::Working, 1, true, true, 60, 128000)));
  led.update(State::Attention, 2, true, true, 60, 129000);
  TEST_ASSERT_EQUAL(int(State::Complete), int(led.update(State::Off, 0, true, true, 60, 130000)));
  led.reset();
  led.update(State::Working, 1, true, true, 5, UINT32_MAX - 1000);
  TEST_ASSERT_EQUAL(int(State::Working), int(led.update(State::Working, 1, true, true, 5, 3998)));
  TEST_ASSERT_EQUAL(int(State::Off), int(led.update(State::Working, 1, true, true, 5, 3999)));
  codex_hid::MonitorSettings settings;
  TEST_ASSERT_TRUE(settings.ledEnabled); TEST_ASSERT_EQUAL(60, settings.ledSeconds);
  uint8_t packet[]{5, 50, 30, 0, 3, 0, 1, 0, 88, 2};
  TEST_ASSERT_TRUE(codex_hid::decodeMonitorSettings(packet, sizeof(packet), settings));
  TEST_ASSERT_FALSE(settings.ledEnabled); TEST_ASSERT_EQUAL(600, settings.ledSeconds);
  const uint8_t legacy[]{4, 40, 20, 0, 6, 1, 0};
  TEST_ASSERT_TRUE(codex_hid::decodeMonitorSettings(legacy, sizeof(legacy), settings));
  TEST_ASSERT_FALSE(settings.ledEnabled); TEST_ASSERT_EQUAL(600, settings.ledSeconds);
  packet[7] = 2;
  TEST_ASSERT_FALSE(codex_hid::decodeMonitorSettings(packet, sizeof(packet), settings));
  TEST_ASSERT_EQUAL(40, settings.volume);
  packet[7] = 1; packet[8] = 4; packet[9] = 0;
  TEST_ASSERT_FALSE(codex_hid::decodeMonitorSettings(packet, sizeof(packet), settings));
  packet[8] = 89; packet[9] = 2;
  TEST_ASSERT_FALSE(codex_hid::decodeMonitorSettings(packet, sizeof(packet), settings));
  TEST_ASSERT_FALSE(codex_hid::decodeMonitorSettings(packet, 9, settings));
}

void test_monitor_ota_rejects_downgrades_and_malformed_identity() {
  using namespace monitor_update_policy;
  TEST_ASSERT_TRUE(newer("0.7.1", "0.7.0"));
  TEST_ASSERT_TRUE(newer("1.0.0", "0.99.99"));
  TEST_ASSERT_FALSE(newer("0.7.0", "0.7.0"));
  TEST_ASSERT_FALSE(newer("0.6.99", "0.7.0"));
  for (auto text : {"-1.0.0", "0.7.1-beta", "0.7", "0.7.1.1", "65536.0.0", "99999999999999.0.0", " 1.0.0", "0..1"})
    TEST_ASSERT_FALSE(newer(text, "0.7.0"));
  TEST_ASSERT_TRUE(hashValid("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"));
  TEST_ASSERT_FALSE(hashValid("0123456789ABCDEF0123456789abcdef0123456789abcdef0123456789abcdef"));
  TEST_ASSERT_FALSE(hashValid("0123456789abcdef"));
  TEST_ASSERT_FALSE(hashValid(nullptr));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_monitor_ota_rejects_downgrades_and_malformed_identity);
  RUN_TEST(test_usb_voice_default_hold_and_separate_toggle);
  RUN_TEST(test_status_led_timeout_priority_and_settings);
  RUN_TEST(test_monitor_settings_v3_validation_and_legacy_preservation);
  RUN_TEST(test_micro_six_keys_and_touch_gaps);
  RUN_TEST(test_usb_loss_preserves_connection_and_last_status);
  RUN_TEST(test_micro_status_mailbox_survives_discovery_and_repaint);
  RUN_TEST(test_monitor_micro_mode_requires_desktop_discovery);
  RUN_TEST(test_codex_hid_descriptor_contract);
  RUN_TEST(test_codex_hid_framing_bounds_and_recovery);
  RUN_TEST(test_codex_hid_discovery_status_events_and_unknown_calls);
  RUN_TEST(test_keypad_touch_boundaries_and_gaps);
  RUN_TEST(test_monitor_voice_capture_gate);
  RUN_TEST(test_monitor_microphone_level);
  RUN_TEST(test_monitor_reset_countdown);
  RUN_TEST(test_recorder_ogg_boundaries_and_corruption);
  RUN_TEST(test_recorder_gate_silence_grace_reset_and_limit);
  RUN_TEST(test_sse_fragmented_status_comments_and_unknown_events);
  RUN_TEST(test_sse_never_delivers_incomplete_or_oversized_events);
  RUN_TEST(test_monitor_timeout_starts_after_active_work_finishes);
  RUN_TEST(test_monitor_touch_and_timeout_setting);
  RUN_TEST(test_monitor_timeout_across_millis_wrap);
  RUN_TEST(test_micro_avatar_status_and_refresh);
  RUN_TEST(test_micro_agent_display_retains_state_across_lighting_off_and_silence);
  RUN_TEST(test_avatar_rgb565_channel_order);
  RUN_TEST(test_avatar_identity_and_state_are_independent);
  RUN_TEST(test_avatar_animation_changes_frame);
  RUN_TEST(test_native_runner_smoke);
  RUN_TEST(test_photo_frame_bmp_validation_and_color);
  RUN_TEST(test_photo_frame_real_decoders_and_cleanup);
  RUN_TEST(test_photo_frame_rejects_invalid_and_exhausted_images);
  RUN_TEST(test_photo_frame_fit_and_alpha);
  RUN_TEST(test_monitor_idle_photos_keep_screen_timeout_and_validate_files);
  RUN_TEST(test_selects_matching_dns_sd_service_and_txt_path);
  RUN_TEST(test_endpoint_falls_back_only_when_configured);
  RUN_TEST(test_service_instance_match_is_exact_and_configured);
  RUN_TEST(test_wav_header_is_mono_16_bit_pcm);
  RUN_TEST(test_wav_header_matches_early_stopped_capture_length);
  RUN_TEST(test_multipart_uses_file_field_and_closing_boundary);
  return UNITY_END();
}
