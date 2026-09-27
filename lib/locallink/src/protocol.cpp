#include "locallink/protocol.hpp"

#include <stdio.h>
#include <string.h>

namespace locallink {
namespace {

bool validEndpoint(const char* host, uint16_t port, const char* path) {
  if (host == nullptr || host[0] == '\0' || port == 0 || path == nullptr ||
      path[0] != '/' || path[1] == '\0') {
    return false;
  }
  for (const char* cursor = host; *cursor != '\0'; ++cursor) {
    if (*cursor <= ' ' || *cursor == '/' || *cursor == '\\' || *cursor == '\r' ||
        *cursor == '\n') {
      return false;
    }
  }
  for (const char* cursor = path; *cursor != '\0'; ++cursor) {
    if (*cursor == '\r' || *cursor == '\n') return false;
  }
  return true;
}

void copyHost(char* destination, const char* source) {
  size_t length = strnlen(source, kHostCapacity - 1);
  while (length > 0 && source[length - 1] == '.') --length;
  memcpy(destination, source, length);
  destination[length] = '\0';
}

bool copyEndpoint(const char* host, uint16_t port, const char* path,
                  bool is_fallback, Endpoint& selected) {
  if (!validEndpoint(host, port, path) ||
      strnlen(host, kHostCapacity) >= kHostCapacity ||
      strnlen(path, kPathCapacity) >= kPathCapacity) {
    return false;
  }
  copyHost(selected.host, host);
  if (selected.host[0] == '\0') return false;
  memcpy(selected.path, path, strlen(path) + 1);
  selected.port = port;
  selected.from_fallback = is_fallback;
  return true;
}

void put16(uint8_t* target, uint16_t value) {
  target[0] = static_cast<uint8_t>(value & 0xFF);
  target[1] = static_cast<uint8_t>((value >> 8) & 0xFF);
}

void put32(uint8_t* target, uint32_t value) {
  target[0] = static_cast<uint8_t>(value & 0xFF);
  target[1] = static_cast<uint8_t>((value >> 8) & 0xFF);
  target[2] = static_cast<uint8_t>((value >> 16) & 0xFF);
  target[3] = static_cast<uint8_t>((value >> 24) & 0xFF);
}

}  // namespace

bool selectEndpoint(const ServiceRecord* records, size_t count,
                    const char* service_instance,
                    const ServiceRecord& fallback, Endpoint& selected) {
  const bool valid_instance =
      service_instance != nullptr && service_instance[0] != '\0' &&
      strnlen(service_instance, kHostCapacity) < kHostCapacity;
  for (size_t i = 0; records != nullptr && i < count; ++i) {
    if (valid_instance &&
        strncmp(records[i].instance, service_instance,
                sizeof(records[i].instance)) == 0 &&
        copyEndpoint(records[i].host, records[i].port, records[i].path, false,
                      selected)) {
      return true;
    }
  }
  return copyEndpoint(fallback.host, fallback.port, fallback.path, true,
                      selected);
}

bool writeWavHeader(uint8_t* header, size_t pcm_bytes, uint32_t sample_rate) {
  if (header == nullptr || pcm_bytes > UINT32_MAX - 36 || sample_rate == 0 ||
      pcm_bytes % 2 != 0 || sample_rate > UINT32_MAX / 2) {
    return false;
  }
  memcpy(header, "RIFF", 4);
  put32(header + 4, static_cast<uint32_t>(pcm_bytes + 36));
  memcpy(header + 8, "WAVEfmt ", 8);
  put32(header + 16, 16);
  put16(header + 20, 1);  // Linear PCM
  put16(header + 22, 1);  // Mono
  put32(header + 24, sample_rate);
  put32(header + 28, sample_rate * 2);  // 16-bit mono byte rate
  put16(header + 32, 2);                // Block alignment
  put16(header + 34, 16);
  memcpy(header + 36, "data", 4);
  put32(header + 40, static_cast<uint32_t>(pcm_bytes));
  return true;
}

size_t writeMultipartPrefix(const char* boundary, char* output,
                            size_t capacity) {
  if (boundary == nullptr || output == nullptr || capacity == 0) return 0;
  const int length = snprintf(
      output, capacity,
      "--%s\r\nContent-Disposition: form-data; name=\"file\"; "
      "filename=\"speech.wav\"\r\nContent-Type: audio/wav\r\n\r\n",
      boundary);
  return length < 0 || static_cast<size_t>(length) >= capacity
             ? 0
             : static_cast<size_t>(length);
}

size_t writeMultipartSuffix(const char* boundary, char* output,
                            size_t capacity) {
  if (boundary == nullptr || output == nullptr || capacity == 0) return 0;
  const int length = snprintf(output, capacity, "\r\n--%s--\r\n", boundary);
  return length < 0 || static_cast<size_t>(length) >= capacity
             ? 0
             : static_cast<size_t>(length);
}

}  // namespace locallink
