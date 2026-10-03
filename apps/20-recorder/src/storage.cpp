#include "storage.hpp"
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_opus_enc.h"
#include "esp_opus_dec.h"
#include "fnk0104b/idf_microphone.hpp"
#include "freertos/task.h"

namespace recorder {
namespace {
constexpr char kDir[] = "/sdcard/recordings";
constexpr uint16_t kPreSkip = 312; // Opus VOIP lookahead: 6.5 ms, in 48 kHz units.
bool validName(const char* name) {
  if (std::strlen(name) != 16 || std::strncmp(name, "REC", 3) || std::strcmp(name + 11, ".opus")) return false;
  for (unsigned i = 3; i < 11; ++i) if (name[i] < '0' || name[i] > '9') return false;
  return true;
}
bool readPage(FILE* file, uint8_t* bytes, Page& page) {
  if (std::fread(bytes, 1, 27, file) != 27 || bytes[26] == 0 || bytes[26] > 6) return false;
  const unsigned segments = bytes[26];
  if (std::fread(bytes + 27, 1, segments, file) != segments) return false;
  size_t payload = 0;
  for (unsigned i = 0; i < segments; ++i) payload += bytes[27 + i];
  if (!payload || payload > kMaxPacket || std::fread(bytes + 27 + segments, 1, payload, file) != payload) return false;
  return parsePage(bytes, 27 + segments + payload, page);
}
}
bool Storage::begin() {
  if (mkdir(kDir, 0755) && errno != EEXIST) return false;
  lock_ = xSemaphoreCreateMutex();
  auto* control = static_cast<StaticQueue_t*>(heap_caps_calloc(1, sizeof(StaticQueue_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
  auto* data = static_cast<uint8_t*>(heap_caps_malloc(128 * sizeof(Block), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!lock_ || !control || !data) return false;
  queue_ = xQueueCreateStatic(128, sizeof(Block), data, control);
  scan();
  // Espressif recommends about 40 KiB for encoder stacks; reserve extra
  // for our block, Ogg page and filesystem calls. Keep it in internal RAM.
  return queue_ && xTaskCreatePinnedToCore(task, "recorder-storage", 49152, this, 3, nullptr, 1) == pdPASS;
}
bool Storage::send(const Block& block) {
  if (!queue_) return false;
  if (block.work == Work::Play) stop_playback_.store(false);
  if (xQueueSend(queue_, &block, 0) == pdTRUE) return true;
  overflow_.store(true);
  return false;
}
StorageStatus Storage::status() {
  xSemaphoreTake(lock_, portMAX_DELAY);
  StorageStatus result = published_;
  xSemaphoreGive(lock_);
  return result;
}
Catalog Storage::catalog() {
  xSemaphoreTake(lock_, portMAX_DELAY);
  Catalog result = catalog_;
  xSemaphoreGive(lock_);
  return result;
}
void Storage::publish() {
  xSemaphoreTake(lock_, portMAX_DELAY);
  published_ = status_;
  xSemaphoreGive(lock_);
}
void Storage::scan() {
  Catalog found;
  DIR* dir = opendir(kDir);
  if (dir) {
    while (auto* entry = readdir(dir)) {
      char candidate[24] = {};
      const size_t length = std::strlen(entry->d_name);
      if (length == 21 && !std::strcmp(entry->d_name + 16, ".part")) {
        std::memcpy(candidate, entry->d_name, 16);
      } else if (length == 16) {
        std::memcpy(candidate, entry->d_name, 16);
      }
      if (validName(candidate)) next_id_ = std::max(next_id_, unsigned(std::strtoul(candidate + 3, nullptr, 10)) + 1);
      if (!validName(entry->d_name)) continue;
      if (found.count < kMaxFiles) std::snprintf(found.names[found.count++], 24, "%.16s", entry->d_name);
      else {
        // Keep the newest bounded set; Next/Previous covers these entries.
        unsigned oldest = 0;
        for (unsigned i = 1; i < found.count; ++i)
          if (std::strcmp(found.names[i], found.names[oldest]) < 0) oldest = i;
        if (std::strcmp(entry->d_name, found.names[oldest]) > 0)
          std::snprintf(found.names[oldest], 24, "%.16s", entry->d_name);
      }
    }
    closedir(dir);
  }
  std::qsort(found.names, found.count, sizeof(found.names[0]), [](const void* a, const void* b) { return std::strcmp(static_cast<const char*>(b), static_cast<const char*>(a)); });
  xSemaphoreTake(lock_, portMAX_DELAY);
  catalog_ = found;
  xSemaphoreGive(lock_);
}
void Storage::error(const char* reason) {
  ESP_LOGE("recorder", "storage_error=%s partial=%s", reason, path_);
  status_.failed = true;
  status_.recording = status_.playing = false;
  std::snprintf(status_.message, sizeof(status_.message), "%s", reason);
  if (file_) { std::fclose(file_); file_ = nullptr; }
  if (encoder_) { esp_opus_enc_close(encoder_); encoder_ = nullptr; }
  fnk0104b::setIdfSpeakerEnabled(false);
  publish();
}
bool Storage::page(const uint8_t* packet, size_t size, uint64_t granule, uint8_t flags) {
  uint8_t bytes[kMaxPage];
  const size_t length = makePage(bytes, sizeof(bytes), packet, size, serial_, sequence_++, granule, flags);
  return length && std::fwrite(bytes, 1, length, file_) == length;
}
bool Storage::start() {
  if (file_ || status_.playing || status_.failed) return false;
  // Exclusive creation also reserves names of interrupted .part files.
  for (unsigned id = next_id_; id < 100000000; ++id) {
    char final[80];
    std::snprintf(final, sizeof(final), "%s/REC%08u.opus", kDir, id);
    struct stat st;
    if (stat(final, &st) == 0) continue;
    std::snprintf(path_, sizeof(path_), "%s/REC%08u.opus.part", kDir, id);
    int fd = open(path_, O_CREAT | O_EXCL | O_WRONLY, 0644);
    if (fd < 0) { if (errno == EEXIST) continue; return false; }
    file_ = fdopen(fd, "wb");
    if (!file_) { close(fd); return false; }
    next_id_ = id + 1;
    std::snprintf(status_.last_file, sizeof(status_.last_file), "REC%08u.opus", id);
    break;
  }
  if (!file_) return false;
  setvbuf(file_, nullptr, _IOFBF, 8192);
  esp_opus_enc_config_t cfg = ESP_OPUS_ENC_CONFIG_DEFAULT();
  cfg.sample_rate = 16000; cfg.channel = 1; cfg.bitrate = 24000;
  cfg.complexity = 0; cfg.enable_vbr = false;
  if (esp_opus_enc_open(&cfg, sizeof(cfg), &encoder_) != ESP_AUDIO_ERR_OK) return false;
  int input_size = 0, output_size = 0;
  if (esp_opus_enc_get_frame_size(encoder_, &input_size, &output_size) != ESP_AUDIO_ERR_OK ||
      input_size != sizeof(pcm_) || output_size > sizeof(packet_)) return false;
  serial_ = esp_random(); sequence_ = 0; used_ = 0;
  samples_ = encoded_samples_ = 0; packet_size_ = 0;
  uint8_t head[19] = {};
  std::memcpy(head, "OpusHead", 8); head[8] = 1; head[9] = 1;
  putLe(head + 10, kPreSkip, 2); putLe(head + 12, 16000, 4);
  uint8_t tags[25] = {};
  std::memcpy(tags, "OpusTags", 8); putLe(tags + 8, 9, 4);
  std::memcpy(tags + 12, "FNK0104B", 8); tags[20] = ' ';
  if (!page(head, sizeof(head), 0, 2) || !page(tags, sizeof(tags), 0, 0)) return false;
  status_.recording = true;
  std::snprintf(status_.message, sizeof(status_.message), "Recording");
  ESP_LOGI("recorder", "recording_started=%s opus=24000 pcm=16000", status_.last_file);
  return true;
}
bool Storage::encode() {
  if (packet_size_ && !page(packet_, packet_size_, packet_granule_, 0)) return false;
  esp_audio_enc_in_frame_t in = {reinterpret_cast<uint8_t*>(pcm_), sizeof(pcm_)};
  esp_audio_enc_out_frame_t out = {};
  out.buffer = packet_; out.len = sizeof(packet_);
  if (esp_opus_enc_process(encoder_, &in, &out) != ESP_AUDIO_ERR_OK || !out.encoded_bytes || out.encoded_bytes > sizeof(packet_)) return false;
  packet_size_ = out.encoded_bytes;
  encoded_samples_ += 320;
  packet_granule_ = encoded_samples_ * 3;
  used_ = 0;
  return true;
}
bool Storage::append(const int16_t* pcm, unsigned count) {
  if (!file_ || !encoder_) return false;
  samples_ += count;
  while (count) {
    unsigned n = std::min(count, 320 - used_);
    std::memcpy(pcm_ + used_, pcm, n * 2);
    used_ += n; pcm += n; count -= n;
    if (used_ == 320 && !encode()) return false;
  }
  return true;
}
bool Storage::finish() {
  if (!file_) return true;
  // Pad the final PCM frame and flush the encoder lookahead. End granule
  // trims padding so players produce exactly the captured sample count.
  do {
    std::fill(pcm_ + used_, pcm_ + 320, 0);
    if (!encode()) return false;
  } while (encoded_samples_ < samples_ + kPreSkip / 3);
  if (!page(packet_, packet_size_, samples_ * 3 + kPreSkip, 4)) return false;
  bool okay = std::fflush(file_) == 0;
  if (okay) okay = fsync(fileno(file_)) == 0;
  if (std::fclose(file_) != 0) okay = false;
  file_ = nullptr;
  esp_opus_enc_close(encoder_); encoder_ = nullptr;
  if (!okay) return false;
  char final[80]; std::snprintf(final, sizeof(final), "%s/%s", kDir, status_.last_file);
  if (rename(path_, final)) return false;
  status_.recording = false; ++status_.saved;
  std::snprintf(status_.message, sizeof(status_.message), "Saved %s", status_.last_file);
  ESP_LOGI("recorder", "saved=%s samples=%llu stack_free_bytes=%u", final, static_cast<unsigned long long>(samples_), unsigned(uxTaskGetStackHighWaterMark(nullptr)));
  scan();
  return true;
}
bool Storage::play(const char* name) {
  if (!validName(name) || file_ || status_.failed) return false;
  char path[80]; std::snprintf(path, sizeof(path), "%s/%s", kDir, name);
  FILE* file = std::fopen(path, "rb");
  if (!file) return false;
  void* decoder = nullptr;
  esp_opus_dec_cfg_t cfg = ESP_OPUS_DEC_CONFIG_DEFAULT();
  cfg.sample_rate = 16000; cfg.channel = 1;
  cfg.frame_duration = ESP_OPUS_DEC_FRAME_DURATION_20_MS;
  uint8_t bytes[kMaxPage]; Page p;
  bool okay = readPage(file, bytes, p) && p.sequence == 0 && p.flags == 2 && p.granule == 0 &&
      p.length == 19 && !std::memcmp(p.packet, "OpusHead", 8) && p.packet[8] == 1 && p.packet[9] == 1 &&
      getLe(p.packet + 10, 2) == kPreSkip && getLe(p.packet + 12, 4) == 16000 && !getLe(p.packet + 16, 2) && !p.packet[18];
  const uint32_t serial = p.serial;
  if (okay) okay = readPage(file, bytes, p) && p.serial == serial && p.sequence == 1 && p.flags == 0 &&
      p.length >= 16 && !std::memcmp(p.packet, "OpusTags", 8);
  if (okay) okay = esp_opus_dec_open(&cfg, sizeof(cfg), &decoder) == ESP_AUDIO_ERR_OK;
  if (okay) okay = fnk0104b::setIdfSpeakerEnabled(true) == ESP_OK;
  status_.playing = okay;
  std::snprintf(status_.message, sizeof(status_.message), "Playing %s", name);
  publish();
  uint64_t decoded = 0;
  uint32_t sequence = 2;
  bool ended = false;
  while (okay && !ended && !stop_playback_.load()) {
    okay = readPage(file, bytes, p) && p.serial == serial && p.sequence == sequence++ && !(p.flags & 2);
    if (!okay) break;
    int16_t pcm[320];
    esp_audio_dec_in_raw_t raw = {};
    raw.buffer = const_cast<uint8_t*>(p.packet); raw.len = p.length;
    esp_audio_dec_out_frame_t out = {};
    out.buffer = reinterpret_cast<uint8_t*>(pcm); out.len = sizeof(pcm);
    esp_audio_dec_info_t info = {};
    okay = esp_opus_dec_decode(decoder, &raw, &out, &info) == ESP_AUDIO_ERR_OK &&
        raw.consumed == p.length && out.decoded_size == sizeof(pcm) && info.sample_rate == 16000 && info.channel == 1;
    if (!okay) break;
    ended = p.flags & 4;
    // Granules use 48 kHz units; PCM and pre-skip here use 16 kHz.
    const uint64_t end = ended ? p.granule / 3 : decoded + 320;
    if ((ended && (p.granule < kPreSkip || p.granule % 3 || end < decoded || end > decoded + 320)) ||
        (!ended && p.granule != (decoded + 320) * 3)) { okay = false; break; }
    const uint64_t begin = std::max(decoded, uint64_t(kPreSkip / 3));
    if (end > begin) okay = fnk0104b::writeIdfSpeaker(pcm + (begin - decoded), end - begin) == ESP_OK;
    decoded += 320;
  }
  // Drain queued DMA data before muting. Recognition remains suspended.
  if (okay && ended) vTaskDelay(pdMS_TO_TICKS(150));
  fnk0104b::setIdfSpeakerEnabled(false);
  if (decoder) esp_opus_dec_close(decoder);
  std::fclose(file);
  status_.playing = false;
  std::snprintf(status_.message, sizeof(status_.message), "%s", okay ? "Playback stopped" : "File unreadable or damaged");
  ESP_LOGI("recorder", "playback=%s file=%s", okay ? "done" : "failed", name);
  return true; // A damaged file must not prevent subsequent recordings.
}
void Storage::task(void* context) {
  auto& self = *static_cast<Storage*>(context);
  Block block;
  while (true) {
    if (self.overflow_.exchange(false)) self.error("Capture interrupted; partial retained");
    if (xQueueReceive(self.queue_, &block, pdMS_TO_TICKS(100)) != pdTRUE) continue;
    if (self.status_.failed) continue;
    bool okay = true;
    switch (block.work) {
      case Work::Start: okay = self.start(); break;
      case Work::Audio: okay = self.append(block.samples, block.count); break;
      case Work::Finish: okay = self.finish(); break;
      case Work::Play: okay = self.play(block.name); break;
      case Work::StopPlayback: self.stop_playback_.store(true); break;
    }
    if (!okay) self.error("SD or audio codec operation failed");
    ++self.status_.completed;
    self.publish();
  }
}
}
