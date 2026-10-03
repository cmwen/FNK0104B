#pragma once
#include <atomic>
#include "esp_afe_sr_models.h"

// The feed task owns microphone reads. The main task consumes continuous AFE
// output, including silence, so VAD delay/cache never clips command prefixes.
class AudioFrontEnd {
 public:
  bool begin(srmodel_list_t* models, char* vad_model);
  afe_fetch_result_t* fetch();
  int fetchSamples() const;
  esp_err_t error() const { return error_.load(); }
  unsigned fedFrames() const { return fed_frames_.load(); }
 private:
  static void feedTask(void* context);
  const esp_afe_sr_iface_t* iface_ = nullptr;
  esp_afe_sr_data_t* data_ = nullptr;
  int feed_samples_ = 0;
  int16_t* feed_buffer_ = nullptr;
  std::atomic<esp_err_t> error_{ESP_OK};
  std::atomic<unsigned> fed_frames_{0};
};
