#pragma once

#include <stddef.h>
#include <string.h>

namespace locallink {

// Bounded, incremental SSE framing. The owner applies complete status events;
// comments, unknown events and incomplete JSON never become status snapshots.
template <size_t Capacity>
class StatusEventParser {
 public:
  enum class Result { None, Status, Overflow };

  Result feed(char c) {
    if (delivered_) { dataSize_ = 0; delivered_ = false; }
    if (afterCr_) { afterCr_ = false; if (c == '\n') return Result::None; }
    if (c == '\r' || c == '\n') {
      afterCr_ = c == '\r';
      const Result result = finishLine();
      lineSize_ = 0;
      return result;
    }
    if (lineSize_ == Capacity) return Result::Overflow;
    line_[lineSize_++] = c;
    return Result::None;
  }

  const char* data() const { return data_; }
  size_t size() const { return dataSize_; }

 private:
  Result finishLine() {
    line_[lineSize_] = '\0';
    if (!lineSize_) {
      const bool status = !strcmp(event_, "status") && dataSize_;
      event_[0] = '\0';
      if (status) {
        --dataSize_;  // Remove the final data-line newline, per SSE framing.
        data_[dataSize_] = '\0';
        delivered_ = true;
        return Result::Status;
      }
      dataSize_ = 0;
      return Result::None;
    }
    if (line_[0] == ':') return Result::None;
    char* colon = strchr(line_, ':');
    char* value = line_ + lineSize_;
    if (colon) {
      *colon = '\0'; value = colon + 1;
      if (*value == ' ') ++value;
    }
    if (!strcmp(line_, "event")) {
      const size_t length = strlen(value);
      if (length < sizeof(event_)) memcpy(event_, value, length + 1);
      else strcpy(event_, "unknown");
    } else if (!strcmp(line_, "data")) {
      const size_t length = strlen(value);
      if (length + 1 > Capacity - dataSize_) return Result::Overflow;
      memcpy(data_ + dataSize_, value, length);
      dataSize_ += length;
      data_[dataSize_++] = '\n';
    }
    return Result::None;
  }

  char line_[Capacity + 1]{};
  char data_[Capacity + 1]{};
  char event_[32]{};
  size_t lineSize_ = 0, dataSize_ = 0;
  bool afterCr_ = false, delivered_ = false;
};

}  // namespace locallink
