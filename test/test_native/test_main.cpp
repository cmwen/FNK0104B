#include <unity.h>

#include <string.h>

#include <locallink/protocol.hpp>

void test_native_runner_smoke() {
  TEST_ASSERT_EQUAL_INT(42, 6 * 7);
}

void test_selects_matching_dns_sd_service_and_txt_path() {
  locallink::ServiceRecord records[2] = {};
  strcpy(records[0].instance, "LocalLink Speech Recognition");
  strcpy(records[0].host, "other.local.");
  strcpy(records[0].path, "/wrong");
  records[0].port = 8080;
  strcpy(records[1].instance, "Speech Recognition");
  strcpy(records[1].host, "speech-locallink.local.");
  strcpy(records[1].path, "/v1/audio/transcriptions");
  records[1].port = 8081;

  locallink::ServiceRecord fallback = {};
  locallink::Endpoint selected = {};
  TEST_ASSERT_TRUE(locallink::selectEndpoint(
      records, 2, "Speech Recognition", fallback, selected));
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
      nullptr, 0, "Speech Recognition", fallback, selected));
  TEST_ASSERT_TRUE(selected.from_fallback);
  TEST_ASSERT_EQUAL_STRING("192.168.1.32", selected.host);
  TEST_ASSERT_EQUAL_STRING("/v1/audio/transcriptions", selected.path);
  TEST_ASSERT_EQUAL_UINT16(8081, selected.port);

  memset(&fallback, 0, sizeof(fallback));
  TEST_ASSERT_FALSE(locallink::selectEndpoint(
      nullptr, 0, "Speech Recognition", fallback, selected));
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

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_native_runner_smoke);
  RUN_TEST(test_selects_matching_dns_sd_service_and_txt_path);
  RUN_TEST(test_endpoint_falls_back_only_when_configured);
  RUN_TEST(test_service_instance_match_is_exact_and_configured);
  RUN_TEST(test_wav_header_is_mono_16_bit_pcm);
  RUN_TEST(test_wav_header_matches_early_stopped_capture_length);
  RUN_TEST(test_multipart_uses_file_field_and_closing_boundary);
  return UNITY_END();
}
