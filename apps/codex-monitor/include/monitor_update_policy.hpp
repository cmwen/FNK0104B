#pragma once
#include <cstdint>
#include <cstring>
namespace monitor_update_policy {
inline bool parseVersion(const char* text, uint32_t (&parts)[3]) {
  if (!text) return false;
  for (int i = 0; i < 3; ++i) {
    if (*text < '0' || *text > '9') return false;
    uint32_t n = 0;
    do {
      n = n * 10 + uint32_t(*text++ - '0');
      if (n > 65535) return false;
    } while (*text >= '0' && *text <= '9');
    parts[i] = n;
    if (i < 2) {
      if (*text++ != '.') return false;
    }
  }
  return *text == 0;
}
inline bool newer(const char* next, const char* current) {
  uint32_t a[3], b[3];
  if (!parseVersion(next, a) || !parseVersion(current, b)) return false;
  for (int i = 0; i < 3; ++i)
    if (a[i] != b[i]) return a[i] > b[i];
  return false;
}
inline bool hashValid(const char* value) {
  if (!value || std::strlen(value) != 64) return false;
  for (unsigned i = 0; i < 64; ++i)
    if (!((value[i] >= '0' && value[i] <= '9') ||
          (value[i] >= 'a' && value[i] <= 'f')))
      return false;
  return true;
}
}  // namespace monitor_update_policy
