#pragma once
#include <atomic>
#include <cstdint>
#include <cstdio>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "recorder/ogg.hpp"
namespace recorder {
constexpr size_t kMaxFiles = 128;
struct Catalog { char names[kMaxFiles][24] = {}; unsigned count = 0; };
enum class Work { Start, Audio, Finish, Play, StopPlayback };
struct Block { Work work; unsigned count = 0; int16_t samples[512] = {}; char name[24] = {}; };
struct StorageStatus { bool recording = false; bool playing = false; bool failed = false; unsigned saved = 0; unsigned completed = 0; char message[48] = {}; char last_file[24] = {}; };
class Storage {
 public:
  bool begin();
  bool send(const Block& block);
  void abort() { stop_playback_.store(true); overflow_.store(true); }
  void stopPlayback() { stop_playback_.store(true); }
  StorageStatus status();
  Catalog catalog();
 private:
  static void task(void* context);
  bool start();
  bool append(const int16_t* pcm, unsigned count);
  bool encode();
  bool finish();
  bool page(const uint8_t* packet, size_t size, uint64_t granule, uint8_t flags);
  bool play(const char* name);
  void scan();
  void publish();
  void error(const char* reason);
  QueueHandle_t queue_ = nullptr;
  SemaphoreHandle_t lock_ = nullptr;
  StorageStatus status_;
  StorageStatus published_;
  std::atomic<bool> overflow_{false};
  Catalog catalog_;
  std::atomic<bool> stop_playback_{false};
  FILE* file_ = nullptr;
  void* encoder_ = nullptr;
  int16_t pcm_[320] = {};
  unsigned used_ = 0;
  uint64_t samples_ = 0, encoded_samples_ = 0;
  uint8_t packet_[recorder::kMaxPacket] = {};
  unsigned packet_size_ = 0;
  uint64_t packet_granule_ = 0;
  uint32_t serial_ = 0, sequence_ = 0;
  unsigned next_id_ = 1;
  char path_[80] = {};
};
}
