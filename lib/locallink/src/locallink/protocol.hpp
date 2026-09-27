#pragma once

#include <stddef.h>
#include <stdint.h>

namespace locallink {

constexpr size_t kHostCapacity = 96;
constexpr size_t kPathCapacity = 128;

struct ServiceRecord {
  char instance[kHostCapacity];
  char host[kHostCapacity];
  char path[kPathCapacity];
  uint16_t port;
};

struct Endpoint {
  char host[kHostCapacity];
  char path[kPathCapacity];
  uint16_t port;
  bool from_fallback;
};

bool selectEndpoint(const ServiceRecord* records, size_t count,
                    const char* service_instance,
                    const ServiceRecord& fallback, Endpoint& selected);
bool writeWavHeader(uint8_t* header, size_t pcm_bytes, uint32_t sample_rate);
size_t writeMultipartPrefix(const char* boundary, char* output, size_t capacity);
size_t writeMultipartSuffix(const char* boundary, char* output, size_t capacity);

}  // namespace locallink
