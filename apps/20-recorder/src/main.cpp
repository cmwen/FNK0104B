#include <algorithm>
#include <cstdio>
#include <cstring>
#include "audio_frontend.hpp"
#include "storage.hpp"
#include "screen.hpp"
#include "dl_kernel.hpp"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_wn_models.h"
#include "model_path.h"
#include "fnk0104b/idf_microphone.hpp"
#include "fnk0104b/idf_storage.hpp"
#include "freertos/task.h"
namespace {
recorder_screen::Snapshot screen;
recorder::Storage storage;
AudioFrontEnd frontend;
void fail(const char* message) {
  ESP_LOGE("recorder", "failed=%s", message);
  storage.abort();
  screen.phase = recorder_screen::Phase::Error;
  std::snprintf(screen.message, sizeof(screen.message), "%s", message);
  recorder_screen::publish(screen);
  fnk0104b::setIdfSpeakerEnabled(false);
  while (true) vTaskDelay(pdMS_TO_TICKS(1000));
}
}
extern "C" void app_main() {
  using recorder_screen::Phase;
  if (!recorder_screen::begin()) fail("Display initialization failed");
  // Audio creates the shared I2C bus, touch attaches to it once.
  esp_err_t err = fnk0104b::beginIdfAudio();
  if (err != ESP_OK) fail(esp_err_to_name(err));
  if ((err = fnk0104b::beginIdfTouch()) != ESP_OK) fail("Touch initialization failed");
  recorder_screen::enableTouch();
  if ((err = fnk0104b::mountIdfSdCard()) != ESP_OK) fail("Insert FAT32 SD card and restart");
  if (!storage.begin()) fail("Cannot create recordings folder");
  if (!dl_kernel_lookup("dl_tie728_w8a16_conv2d_11cn")) fail("WakeNet kernel missing");
  srmodel_list_t* models = esp_srmodel_init("model");
  if (!models) fail("Speech models unavailable");
  char* wake_name = esp_srmodel_filter(models, "wn10", "hiesp");
  char* vad_name = esp_srmodel_filter(models, ESP_VADN_PREFIX, "medium");
  if (!wake_name || !vad_name) fail("WakeNet or VADNet missing");
  const esp_wn_iface_t* wake = esp_wn_handle_from_name(wake_name);
  if (!wake) fail("WakeNet interface missing");
  model_iface_data_t* wake_data = wake->create(wake_name, DET_MODE_90);
  if (!wake_data) fail("WakeNet allocation failed");
  if (!frontend.begin(models, vad_name)) fail("VADNet initialization failed");
  if (frontend.fetchSamples() != 512 || wake->get_samp_chunksize(wake_data) != 512 ||
      wake->get_samp_rate(wake_data) != 16000 || wake->get_channel_num(wake_data) != 1) fail("Unexpected audio frame size");
  auto* history = static_cast<int16_t*>(heap_caps_calloc(8192, 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!history) fail("Pre-roll allocation failed");
  unsigned history_frames = 0, history_next = 0;
  recorder::RecordingGate gate;
  auto catalog = storage.catalog();
  unsigned selected = 0;
  unsigned saved_before = 0, completed_before = 0;
  int64_t last_ui = 0, last_report = 0, cooldown = 0;
  int64_t max_us = 0;
  screen.phase = Phase::Ready;
  std::snprintf(screen.message, sizeof(screen.message), "HI ESP then speak. Silence saves.");
  ESP_LOGI("recorder", "ready wake=Hi_ESP vad=%s opus=24000 sd=mounted silence_hold_ms=%u",
      vad_name, recorder::RecordingGate::kSilenceHoldMs);
  while (true) {
    afe_fetch_result_t* audio = frontend.fetch();
    if (frontend.error() != ESP_OK || !audio || audio->ret_value == ESP_FAIL ||
        !audio->data || audio->data_size != 1024) fail("Audio capture failed; restart");
    const int64_t started = esp_timer_get_time();
    const Phase previous_phase = screen.phase;
    bool handled_touch = false;
    auto status = storage.status();
    if (status.failed) fail(status.message);
    if (screen.phase == Phase::Saving && status.saved > saved_before) {
      catalog = storage.catalog(); selected = 0;
      screen.phase = Phase::Ready;
      std::snprintf(screen.message, sizeof(screen.message), "%s", status.message);
      cooldown = started + 1500000;
      wake->clean(wake_data);
    }
    if (screen.phase == Phase::Playing && status.completed > completed_before && !status.playing) {
      screen.phase = Phase::Ready;
      std::snprintf(screen.message, sizeof(screen.message), "%s", status.message);
      cooldown = started + 1500000;
      history_frames = 0; history_next = 0;
      wake->clean(wake_data);
    }
    bool start = false, stop = false;
    recorder_screen::Event event;
    while (recorder_screen::poll(event)) {
      handled_touch = true;
      ESP_LOGI("recorder-ui", "touch_action=%d row=%u event_age_us=%lld phase=%d",
          int(event.action), event.row, (long long)(esp_timer_get_time() - event.sampled_us), int(screen.phase));
      using recorder_screen::Action;
      if (event.action == Action::Record) {
        if (gate.active()) stop = true;
        else if (screen.phase == Phase::Ready) start = true;
      } else if (event.action == Action::Play) {
        if (screen.phase == Phase::Playing) storage.stopPlayback();
        else if (screen.phase == Phase::Ready && catalog.count && !start) {
          recorder::Block block{recorder::Work::Play};
          std::snprintf(block.name, sizeof(block.name), "%s", catalog.names[selected]);
          completed_before = status.completed;
          if (!storage.send(block)) fail("Playback queue full");
          screen.phase = Phase::Playing;
          screen.seconds = 0;
          std::snprintf(screen.message, sizeof(screen.message), "Playing %.16s", block.name);
          wake->clean(wake_data);
        }
      } else if (screen.phase == Phase::Ready && catalog.count) {
        if (event.action == Action::Previous) selected = selected >= 3 ? selected - 3 : 0;
        if (event.action == Action::Next) selected = std::min(catalog.count - 1, selected + 3);
        if (event.action == Action::Select) selected = std::min(catalog.count - 1, selected / 3 * 3 + event.row);
      }
    }
    screen.speech = audio->vad_state == VAD_SPEECH;
    screen.peak = 0;
    for (unsigned i = 0; i < 512; ++i) screen.peak = std::max(screen.peak, std::abs(int(audio->data[i])));
    if (screen.phase == Phase::Ready && started >= cooldown && wake->detect(wake_data, audio->data) == WAKENET_DETECTED) start = true;
    if (start && screen.phase == Phase::Ready) {
      if (!storage.send({recorder::Work::Start})) fail("Recording queue full");
      gate.start(); screen.phase = Phase::Recording;
      std::snprintf(screen.message, sizeof(screen.message), "Silence saves. Tap STOP to save now.");
      // Continuous AFE pre-roll includes 512 ms before trigger. No vad_cache
      // append: those samples were already collected and would be duplicated.
      for (unsigned i = 0; i < history_frames; ++i) {
        recorder::Block block{recorder::Work::Audio}; block.count = 512;
        const unsigned index = (history_next + 16 - history_frames + i) % 16;
        std::memcpy(block.samples, history + index * 512, 1024);
        if (!storage.send(block)) fail("Audio queue overrun");
      }
    }
    if (gate.active()) {
      recorder::Block block{recorder::Work::Audio}; block.count = 512;
      std::memcpy(block.samples, audio->data, 1024);
      if (!storage.send(block)) fail("Audio queue overrun; partial retained");
      stop |= gate.feed(screen.speech, 512);
      screen.seconds = gate.samples() / 16000;
      if (stop) {
        saved_before = status.saved;
        if (!storage.send({recorder::Work::Finish})) fail("Save queue full; partial retained");
        gate.stop(); screen.phase = Phase::Saving;
        std::snprintf(screen.message, sizeof(screen.message), "Finishing recording on SD card");
        wake->clean(wake_data);
      }
    }
    if (screen.phase == Phase::Ready || screen.phase == Phase::Recording) {
      std::memcpy(history + history_next * 512, audio->data, 1024);
      history_next = (history_next + 1) % 16;
      history_frames = std::min(16u, history_frames + 1);
    }
    max_us = std::max(max_us, esp_timer_get_time() - started);
    if (handled_touch || screen.phase != previous_phase || started - last_ui >= 100000) {
      screen.count = catalog.count; screen.page = selected / 3; screen.selected_row = selected % 3;
      for (unsigned i = 0; i < 3; ++i) {
        unsigned index = screen.page * 3 + i;
        std::snprintf(screen.files[i], sizeof(screen.files[i]), "%s", index < catalog.count ? catalog.names[index] : "");
      }
      recorder_screen::publish(screen); last_ui = started;
    }
    if (started - last_report >= 5000000) {
      ESP_LOGI("recorder", "phase=%d vad=%s peak=%d max_consume_us=%lld frame_us=32000 fed=%u heap=%u psram=%u",
          int(screen.phase), screen.speech ? "speech" : "silence", screen.peak, (long long)max_us,
          frontend.fedFrames(), unsigned(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)), unsigned(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)));
      max_us = 0; last_report = started;
    }
    vTaskDelay(1);
  }
}
