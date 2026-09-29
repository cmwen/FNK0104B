#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <ESPmDNS.h>
#include <Wire.h>
#include <lvgl.h>
#include <ArduinoJson.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <mdns.h>

#include <fnk0104b/board.hpp>
#include <fnk0104b/pins.hpp>
#include <locallink/protocol.hpp>

#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <atomic>

#include "locallink_config.h"

namespace {

constexpr char kFirmwareVersion[] = "0.2.5";
constexpr char kBoundary[] = "----FNK0104BLocalLink7MA4YWxkTrZu0gW";
constexpr uint32_t kSampleRate = 16000;
constexpr uint32_t kDiscoveryTimeoutMs = 1800;
constexpr uint32_t kConnectTimeoutMs = 5000;
constexpr uint16_t kResponseTimeoutMs = 45000;
constexpr size_t kMaxServiceResults = 20;
constexpr size_t kMaxResponseBytes = 8192;
constexpr size_t kTranscriptionCapacity = 640;
constexpr size_t kErrorCapacity = 96;
constexpr int32_t kScreenWidth = 320;
constexpr uint32_t kRecordButtonDebounceMs = 30;

static_assert(LOCALLINK_RECORD_SECONDS >= 1 && LOCALLINK_RECORD_SECONDS <= 20,
              "Recording duration must be between 1 and 20 seconds");

constexpr uint32_t kBackground = 0x101827;
constexpr uint32_t kPanel = 0x1C2838;
constexpr uint32_t kBlue = 0x2478D4;
constexpr uint32_t kGreen = 0x168C69;
constexpr uint32_t kRed = 0xA93242;
constexpr uint32_t kText = 0xF1F5F9;
constexpr uint32_t kMuted = 0xA9B7C7;
constexpr uint32_t kAccent = 0x58C7D9;

enum class UiState : uint8_t {
  kStarting,
  kReady,
  kRecording,
  kDiscovering,
  kUploading,
  kComplete,
  kError,
};

struct UiSnapshot {
  UiState state;
  char transcription[kTranscriptionCapacity];
  char error[kErrorCapacity];
};

portMUX_TYPE ui_mux = portMUX_INITIALIZER_UNLOCKED;
UiState ui_state = UiState::kStarting;
char transcription[kTranscriptionCapacity] = {};
char ui_error[kErrorCapacity] = {};
TaskHandle_t speech_worker = nullptr;
bool mdns_started = false;
bool microphone_ready = false;
fnk0104b::TouchPoint touch_point{0, 0, false};
std::atomic<bool> capture_stop_requested{false};
std::atomic<bool> gpio_record_button_held{false};
std::atomic<bool> gpio_record_start_pending{false};
std::atomic<bool> gpio_record_capture_active{false};
std::atomic<uint8_t> microphone_level{0};
bool gpio_record_button_stable_pressed = false;
bool gpio_record_button_last_reading_pressed = false;
uint32_t gpio_record_button_last_change_ms = 0;

lv_display_t* lv_display = nullptr;
lv_indev_t* lv_touch = nullptr;
lv_obj_t* status_label = nullptr;
lv_obj_t* wifi_label = nullptr;
lv_obj_t* result_label = nullptr;
lv_obj_t* microphone_bar = nullptr;
lv_obj_t* record_button = nullptr;
lv_obj_t* record_button_label = nullptr;

TFT_eSPI& displayDriver() { return fnk0104b::display.driver(); }

void setState(UiState state, const char* message = nullptr) {
  portENTER_CRITICAL(&ui_mux);
  ui_state = state;
  if (state == UiState::kComplete && message != nullptr) {
    strlcpy(transcription, message, sizeof(transcription));
    ui_error[0] = '\0';
  } else if (state == UiState::kError) {
    strlcpy(ui_error, message == nullptr ? "Request failed" : message,
            sizeof(ui_error));
  } else if (state == UiState::kReady) {
    ui_error[0] = '\0';
  }
  portEXIT_CRITICAL(&ui_mux);
}

UiSnapshot getSnapshot() {
  UiSnapshot snapshot{};
  portENTER_CRITICAL(&ui_mux);
  snapshot.state = ui_state;
  strlcpy(snapshot.transcription, transcription, sizeof(snapshot.transcription));
  strlcpy(snapshot.error, ui_error, sizeof(snapshot.error));
  portEXIT_CRITICAL(&ui_mux);
  return snapshot;
}

void flushDisplay(lv_display_t* display, const lv_area_t* area,
                  uint8_t* pixel_map) {
  const uint32_t width = static_cast<uint32_t>(area->x2 - area->x1 + 1);
  const uint32_t height = static_cast<uint32_t>(area->y2 - area->y1 + 1);
  TFT_eSPI& tft = displayDriver();
  tft.setSwapBytes(true);
  tft.startWrite();
  tft.pushImage(area->x1, area->y1, width, height,
                reinterpret_cast<uint16_t*>(pixel_map));
  tft.endWrite();
  lv_display_flush_ready(display);
}

void readTouch(lv_indev_t*, lv_indev_data_t* data) {
  if (!fnk0104b::touch.read(touch_point)) {
    data->state = LV_INDEV_STATE_RELEASED;
    return;
  }
  data->state = touch_point.pressed ? LV_INDEV_STATE_PRESSED
                                    : LV_INDEV_STATE_RELEASED;
  data->point.x = touch_point.x;
  data->point.y = touch_point.y;
}

lv_obj_t* makeLabel(lv_obj_t* parent, const char* value, int32_t x, int32_t y,
                    int32_t width, int32_t height, const lv_font_t* font,
                    uint32_t color) {
  lv_obj_t* label = lv_label_create(parent);
  lv_label_set_text(label, value);
  lv_obj_set_pos(label, x, y);
  lv_obj_set_size(label, width, height);
  lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
  lv_obj_set_style_text_color(label, lv_color_hex(color), LV_PART_MAIN);
  lv_obj_set_scrollable(label, false);
  return label;
}

void onRecordClicked(lv_event_t*) {
  if (speech_worker == nullptr || !microphone_ready) return;
  const UiSnapshot snapshot = getSnapshot();
  if (snapshot.state == UiState::kRecording) {
    capture_stop_requested.store(true, std::memory_order_relaxed);
    Serial.println("speech_button=stop");
    return;
  }
  if (WiFi.status() != WL_CONNECTED ||
      snapshot.state == UiState::kDiscovering ||
      snapshot.state == UiState::kUploading) {
    return;
  }
  capture_stop_requested.store(false, std::memory_order_relaxed);
  gpio_record_start_pending.store(false, std::memory_order_relaxed);
  Serial.println("speech_button=record");
  xTaskNotifyGive(speech_worker);
}

void onRecordButtonPressed() {
  gpio_record_button_held.store(true, std::memory_order_relaxed);
  if (speech_worker == nullptr || !microphone_ready ||
      WiFi.status() != WL_CONNECTED) {
    return;
  }

  const UiState state = getSnapshot().state;
  if (state == UiState::kStarting || state == UiState::kRecording ||
      state == UiState::kDiscovering || state == UiState::kUploading) {
    return;
  }

  capture_stop_requested.store(false, std::memory_order_relaxed);
  gpio_record_start_pending.store(true, std::memory_order_relaxed);
  Serial.println("speech_gpio_button=pressed; recording=started");
  xTaskNotifyGive(speech_worker);
}

void onRecordButtonReleased() {
  gpio_record_button_held.store(false, std::memory_order_relaxed);
  if (gpio_record_start_pending.load(std::memory_order_relaxed) ||
      gpio_record_capture_active.load(std::memory_order_relaxed)) {
    capture_stop_requested.store(true, std::memory_order_relaxed);
    Serial.println("speech_gpio_button=released; recording=stop_requested");
  }
}

void pollRecordButton() {
  const bool reading_pressed =
      digitalRead(fnk0104b::pins::expansion::gpio_14) == LOW;
  const uint32_t now = millis();
  if (reading_pressed != gpio_record_button_last_reading_pressed) {
    gpio_record_button_last_reading_pressed = reading_pressed;
    gpio_record_button_last_change_ms = now;
  }
  if (reading_pressed != gpio_record_button_stable_pressed &&
      now - gpio_record_button_last_change_ms >= kRecordButtonDebounceMs) {
    gpio_record_button_stable_pressed = reading_pressed;
    if (reading_pressed) {
      onRecordButtonPressed();
    } else {
      onRecordButtonReleased();
    }
  }
}

bool shouldStopAudioCapture(void*) {
  return capture_stop_requested.load(std::memory_order_relaxed);
}

void updateMicrophoneLevel(int32_t peak, void*) {
  const int32_t scaled = peak <= 64 ? 0 : (peak - 64) * 100 / 2048;
  const uint8_t level = static_cast<uint8_t>(constrain(scaled, 0, 100));
  const uint8_t previous = microphone_level.load(std::memory_order_relaxed);
  microphone_level.store(level > previous ? level
                                          : (previous * 3 + level) / 4,
                         std::memory_order_relaxed);
}

void initializeLvgl() {
  TFT_eSPI& tft = displayDriver();
  lv_init();
  lv_tick_set_cb([]() -> uint32_t { return millis(); });
  lv_display = lv_display_create(tft.width(), tft.height());
  lv_display_set_color_format(lv_display, LV_COLOR_FORMAT_RGB565);
  static uint16_t draw_buffer[kScreenWidth * 20];
  lv_display_set_buffers(lv_display, draw_buffer, nullptr, sizeof(draw_buffer),
                         LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(lv_display, flushDisplay);
  lv_theme_t* theme = lv_theme_default_init(
      lv_display, lv_color_hex(kBlue), lv_color_hex(kAccent), true,
      LV_FONT_DEFAULT);
  lv_display_set_theme(lv_display, theme);

  lv_touch = lv_indev_create();
  lv_indev_set_type(lv_touch, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(lv_touch, readTouch);
  lv_timer_set_period(lv_indev_get_read_timer(lv_touch), 25);

  lv_obj_t* screen = lv_screen_active();
  lv_obj_set_style_bg_color(screen, lv_color_hex(kBackground), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_pad_all(screen, 0, LV_PART_MAIN);
  lv_obj_set_scrollable(screen, false);

  makeLabel(screen, "LOCALLINK SPEECH", 10, 6, 220, 24,
            &lv_font_montserrat_16, kText);
  wifi_label = makeLabel(screen, "Wi-Fi  waiting", 10, 32, 300, 20,
                         &lv_font_montserrat_12, kMuted);
  status_label = makeLabel(screen, "Starting microphone...", 10, 56, 300, 22,
                           &lv_font_montserrat_14, kAccent);

  record_button = lv_button_create(screen);
  lv_obj_set_pos(record_button, 198, 4);
  lv_obj_set_size(record_button, 112, 48);
  lv_obj_set_style_bg_color(record_button, lv_color_hex(kGreen), LV_PART_MAIN);
  lv_obj_set_style_border_width(record_button, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(record_button, 6, LV_PART_MAIN);
  record_button_label = lv_label_create(record_button);
  lv_label_set_text(record_button_label, "RECORD");
  lv_obj_center(record_button_label);
  lv_obj_add_event_cb(record_button, onRecordClicked, LV_EVENT_PRESSED, nullptr);

  makeLabel(screen, "MIC", 10, 85, 38, 19, &lv_font_montserrat_12, kMuted);
  microphone_bar = lv_bar_create(screen);
  lv_obj_set_pos(microphone_bar, 51, 88);
  lv_obj_set_size(microphone_bar, 259, 12);
  lv_bar_set_range(microphone_bar, 0, 100);
  lv_bar_set_value(microphone_bar, 0, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(microphone_bar, lv_color_hex(kPanel), LV_PART_MAIN);
  lv_obj_set_style_bg_color(microphone_bar, lv_color_hex(kGreen),
                            LV_PART_INDICATOR);

  lv_obj_t* result_panel = lv_obj_create(screen);
  lv_obj_set_pos(result_panel, 8, 110);
  lv_obj_set_size(result_panel, 304, 119);
  lv_obj_set_style_bg_color(result_panel, lv_color_hex(kPanel), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(result_panel, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(result_panel, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(result_panel, 7, LV_PART_MAIN);
  lv_obj_set_style_pad_all(result_panel, 9, LV_PART_MAIN);
  result_label = lv_label_create(result_panel);
  lv_label_set_text(result_label, "Your transcription will appear here.");
  lv_obj_set_width(result_label, 286);
  lv_obj_set_style_text_font(result_label, &lv_font_montserrat_14,
                             LV_PART_MAIN);
  lv_obj_set_style_text_color(result_label, lv_color_hex(kMuted), LV_PART_MAIN);
  lv_label_set_long_mode(result_label, LV_LABEL_LONG_MODE_WRAP);
  lv_obj_set_scrollable(result_panel, true);
}

void updateUi() {
  const UiSnapshot snapshot = getSnapshot();
  const bool connected = WiFi.status() == WL_CONNECTED;
  const uint8_t level = microphone_level.load(std::memory_order_relaxed);
  static uint8_t last_level = 255;
  if (level != last_level) {
    lv_bar_set_value(microphone_bar, level, LV_ANIM_OFF);
    last_level = level;
  }
  static UiSnapshot last_snapshot{};
  static bool last_connected = false;
  static bool initialized = false;
  if (initialized && connected == last_connected &&
      snapshot.state == last_snapshot.state &&
      strcmp(snapshot.transcription, last_snapshot.transcription) == 0 &&
      strcmp(snapshot.error, last_snapshot.error) == 0) {
    return;
  }
  last_snapshot = snapshot;
  last_connected = connected;
  initialized = true;
  lv_label_set_text(wifi_label, connected ? "Wi-Fi  connected"
                                          : "Wi-Fi  waiting / reconnecting");
  lv_obj_set_style_text_color(wifi_label,
                              lv_color_hex(connected ? kGreen : kMuted),
                              LV_PART_MAIN);

  bool busy = true;
  const char* status = "";
  uint32_t status_color = kAccent;
  switch (snapshot.state) {
    case UiState::kStarting:
      status = microphone_ready ? "Starting LocalLink..."
                                : "Microphone initialization failed";
      busy = true;
      break;
    case UiState::kReady:
      status = connected ? "Ready - hold GPIO button or tap RECORD"
                         : "Waiting for Wi-Fi connection";
      busy = false;
      break;
    case UiState::kRecording:
      status = "Recording... release button or tap STOP";
      busy = true;
      break;
    case UiState::kDiscovering:
      status = "Discovering speech service...";
      busy = true;
      break;
    case UiState::kUploading:
      status = "Sending audio for transcription...";
      busy = true;
      break;
    case UiState::kComplete:
      status = "Transcription complete";
      status_color = kGreen;
      busy = false;
      lv_label_set_text(result_label, snapshot.transcription);
      lv_obj_set_style_text_color(result_label, lv_color_hex(kText),
                                  LV_PART_MAIN);
      break;
    case UiState::kError:
      status = snapshot.error;
      status_color = kRed;
      busy = false;
      break;
  }
  lv_label_set_text(status_label, status);
  lv_obj_set_style_text_color(status_label, lv_color_hex(status_color),
                              LV_PART_MAIN);
  if (record_button != nullptr) {
    const bool recording = snapshot.state == UiState::kRecording;
    lv_label_set_text(record_button_label, recording ? "STOP" : "RECORD");
    if (recording) {
      lv_obj_remove_state(record_button, LV_STATE_DISABLED);
      lv_obj_set_style_bg_color(record_button, lv_color_hex(kRed),
                                LV_PART_MAIN);
    } else if (busy || !microphone_ready || !connected) {
      lv_obj_add_state(record_button, LV_STATE_DISABLED);
      lv_obj_set_style_bg_color(record_button, lv_color_hex(kPanel),
                                LV_PART_MAIN);
    } else {
      lv_obj_remove_state(record_button, LV_STATE_DISABLED);
      lv_obj_set_style_bg_color(record_button, lv_color_hex(kGreen),
                                LV_PART_MAIN);
    }
  }
}

void setError(const char* message) { setState(UiState::kError, message); }

bool startMdns() {
  if (mdns_started) return true;
  char hostname[32] = {};
  snprintf(hostname, sizeof(hostname), "locallink-%06X",
           static_cast<unsigned>(ESP.getEfuseMac() & 0xFFFFFF));
  mdns_started = MDNS.begin(hostname);
  return mdns_started;
}

void copyText(char* destination, size_t capacity, const char* source) {
  if (source == nullptr || capacity == 0) return;
  strlcpy(destination, source, capacity);
}

bool discoverEndpoint(locallink::Endpoint& endpoint) {
  locallink::ServiceRecord discovered[kMaxServiceResults] = {};
  size_t discovered_count = 0;
  if (startMdns()) {
    mdns_result_t* results = nullptr;
    const esp_err_t query_result = mdns_query_ptr(
        "_http", "_tcp", kDiscoveryTimeoutMs, kMaxServiceResults, &results);
    if (query_result == ESP_OK) {
      for (mdns_result_t* result = results;
           result != nullptr && discovered_count < kMaxServiceResults;
           result = result->next) {
        if (result->instance_name == nullptr || result->hostname == nullptr) {
          continue;
        }
        locallink::ServiceRecord& record = discovered[discovered_count];
        copyText(record.instance, sizeof(record.instance), result->instance_name);
        copyText(record.host, sizeof(record.host), result->hostname);
        record.port = result->port;
        for (size_t i = 0; i < result->txt_count; ++i) {
          if (result->txt[i].key != nullptr && result->txt[i].value != nullptr &&
              strcmp(result->txt[i].key, "path") == 0) {
            copyText(record.path, sizeof(record.path), result->txt[i].value);
            break;
          }
        }
        ++discovered_count;
      }
    }
    Serial.printf("dns_sd_query=%d records=%u expected=\"%s\"\n",
                  static_cast<int>(query_result),
                  static_cast<unsigned>(discovered_count),
                  LOCALLINK_SERVICE_INSTANCE);
    for (size_t i = 0; i < discovered_count; ++i) {
      Serial.printf("dns_sd_instance=\"%s\" host=\"%s\" port=%u path=\"%s\"\n",
                    discovered[i].instance, discovered[i].host,
                    static_cast<unsigned>(discovered[i].port),
                    discovered[i].path);
    }
    if (results != nullptr) mdns_query_results_free(results);
  } else {
    Serial.println("dns_sd_start=failed");
  }

  locallink::ServiceRecord fallback{};
  copyText(fallback.host, sizeof(fallback.host), LOCALLINK_FALLBACK_HOST);
  copyText(fallback.path, sizeof(fallback.path), LOCALLINK_FALLBACK_PATH);
  fallback.port = static_cast<uint16_t>(LOCALLINK_FALLBACK_PORT);
  const bool selected = locallink::selectEndpoint(
      discovered, discovered_count, LOCALLINK_SERVICE_INSTANCE, fallback,
      endpoint);
  Serial.printf("dns_sd_endpoint=%s\n",
                selected ? (endpoint.from_fallback ? "fallback" : "discovered")
                         : "none");
  return selected;
}

bool resolveServiceHost(const locallink::Endpoint& endpoint,
                        IPAddress& resolved_address) {
  if (resolved_address.fromString(endpoint.host)) return true;

  char mdns_host[locallink::kHostCapacity] = {};
  strlcpy(mdns_host, endpoint.host, sizeof(mdns_host));
  const size_t host_length = strlen(mdns_host);
  constexpr char kLocalSuffix[] = ".local";
  if (host_length > sizeof(kLocalSuffix) - 1 &&
      strcasecmp(mdns_host + host_length - (sizeof(kLocalSuffix) - 1),
                 kLocalSuffix) == 0) {
    mdns_host[host_length - (sizeof(kLocalSuffix) - 1)] = '\0';
  }
  resolved_address = MDNS.queryHost(mdns_host, kDiscoveryTimeoutMs);
  return resolved_address != IPAddress();
}

void* allocateAudioMemory(size_t bytes) {
  void* allocation = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (allocation == nullptr) {
    allocation = heap_caps_malloc(bytes, MALLOC_CAP_8BIT);
  }
  return allocation;
}

bool recordMultipartBody(uint8_t*& body, size_t& body_size) {
  body = nullptr;
  body_size = 0;
  const size_t requested_samples =
      static_cast<size_t>(kSampleRate) * LOCALLINK_RECORD_SECONDS;
  const size_t max_pcm_bytes = requested_samples * sizeof(int16_t);
  char prefix[256] = {};
  char suffix[96] = {};
  const size_t prefix_size =
      locallink::writeMultipartPrefix(kBoundary, prefix, sizeof(prefix));
  const size_t suffix_size =
      locallink::writeMultipartSuffix(kBoundary, suffix, sizeof(suffix));
  if (prefix_size == 0 || suffix_size == 0) return false;

  int16_t* pcm = static_cast<int16_t*>(allocateAudioMemory(max_pcm_bytes));
  if (pcm == nullptr) return false;

  size_t captured_samples = 0;
  const uint32_t capture_timeout = LOCALLINK_RECORD_SECONDS * 1000U + 3000U;
  const bool captured = fnk0104b::microphone.capture(
      pcm, requested_samples, captured_samples, capture_timeout,
      shouldStopAudioCapture, nullptr, updateMicrophoneLevel, nullptr);
  const bool stopped_early =
      capture_stop_requested.load(std::memory_order_relaxed);
  int32_t peak = 0;
  for (size_t i = 0; i < captured_samples; ++i) {
    const int32_t sample = pcm[i];
    const int32_t magnitude = sample < 0 ? -sample : sample;
    if (magnitude > peak) peak = magnitude;
  }
  Serial.printf("audio_capture=%s samples=%u stopped=%s peak=%ld\n",
                captured ? "complete" : "failed",
                static_cast<unsigned>(captured_samples),
                stopped_early ? "yes" : "no", static_cast<long>(peak));
  if (!captured || (!stopped_early && captured_samples != requested_samples)) {
    free(pcm);
    return false;
  }

  const size_t pcm_bytes = captured_samples * sizeof(int16_t);
  const size_t wav_size = 44 + pcm_bytes;
  uint8_t* audio_wav = static_cast<uint8_t*>(allocateAudioMemory(wav_size));
  if (audio_wav == nullptr) {
    free(pcm);
    return false;
  }
  if (!locallink::writeWavHeader(audio_wav, pcm_bytes, kSampleRate)) {
    free(audio_wav);
    free(pcm);
    return false;
  }
  memcpy(audio_wav + 44, pcm, pcm_bytes);
  free(pcm);

  body_size = prefix_size + wav_size + suffix_size;
  body = static_cast<uint8_t*>(allocateAudioMemory(body_size));
  if (body == nullptr) {
    free(audio_wav);
    return false;
  }
  memcpy(body, prefix, prefix_size);
  memcpy(body + prefix_size, audio_wav, wav_size);
  memcpy(body + prefix_size + wav_size, suffix, suffix_size);
  free(audio_wav);
  return true;
}

bool postForTranscription(const locallink::Endpoint& endpoint,
                          const IPAddress& resolved_address,
                          uint8_t* request_body, size_t request_size,
                          char* result, size_t result_capacity,
                          char* error, size_t error_capacity) {
  class ResolvedWiFiClient final : public WiFiClient {
   public:
    explicit ResolvedWiFiClient(IPAddress address) : address_(address) {}
    int connect(const char*, uint16_t port, int32_t timeout) override {
      return WiFiClient::connect(address_, port, timeout);
    }

   private:
    IPAddress address_;
  } client(resolved_address);

  HTTPClient http;
  http.setConnectTimeout(kConnectTimeoutMs);
  http.setTimeout(kResponseTimeoutMs);
  if (!http.begin(client, endpoint.host, endpoint.port, endpoint.path, false)) {
    strlcpy(error, "HTTP connection could not start", error_capacity);
    return false;
  }
  char content_type[96] = {};
  snprintf(content_type, sizeof(content_type),
           "multipart/form-data; boundary=%s", kBoundary);
  http.addHeader("Content-Type", content_type);
  http.addHeader("Accept", "application/json");
  Serial.printf("http_request=starting ip=%s port=%u bytes=%u\n",
                resolved_address.toString().c_str(), endpoint.port,
                static_cast<unsigned>(request_size));
  const uint32_t request_started_at = millis();
  const int status_code = http.POST(request_body, request_size);
  Serial.printf("http_result=%d elapsed_ms=%lu\n", status_code,
                static_cast<unsigned long>(millis() - request_started_at));
  if (status_code != HTTP_CODE_OK) {
    snprintf(error, error_capacity,
             status_code > 0 ? "HTTP response %d" : "HTTP request failed (%d)",
             status_code);
    http.end();
    return false;
  }

  const int response_size = http.getSize();
  if (response_size < 0) {
    strlcpy(error, "Response length is unknown", error_capacity);
    http.end();
    return false;
  }
  if (response_size > static_cast<int>(kMaxResponseBytes)) {
    strlcpy(error, "Response is too large", error_capacity);
    http.end();
    return false;
  }
  String response = http.getString();
  http.end();
  if (response.length() > kMaxResponseBytes) {
    strlcpy(error, "Response is too large", error_capacity);
    return false;
  }

  StaticJsonDocument<2048> document;
  const DeserializationError parse_error = deserializeJson(document, response);
  if (parse_error) {
    strlcpy(error, "Invalid JSON response", error_capacity);
    return false;
  }
  const char* text = document["text"].as<const char*>();
  if (text == nullptr) {
    strlcpy(error, "Response has no text field", error_capacity);
    return false;
  }
  if (text[0] == '\0') {
    strlcpy(error, "No speech recognized; speak closer to the mic",
            error_capacity);
    return false;
  }
  strlcpy(result, text, result_capacity);
  return true;
}

void speechWorker(void*) {
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    const bool gpio_button_recording =
        gpio_record_start_pending.exchange(false, std::memory_order_relaxed);
    if (!microphone_ready) {
      setError("Microphone is not ready");
      continue;
    }
    if (WiFi.status() != WL_CONNECTED) {
      setError("Wi-Fi is disconnected");
      continue;
    }

    gpio_record_capture_active.store(gpio_button_recording,
                                     std::memory_order_relaxed);
    capture_stop_requested.store(
        gpio_button_recording &&
            !gpio_record_button_held.load(std::memory_order_relaxed),
        std::memory_order_relaxed);
    microphone_level.store(0, std::memory_order_relaxed);
    setState(UiState::kRecording);
    uint8_t* request_body = nullptr;
    size_t request_size = 0;
    const bool recorded = recordMultipartBody(request_body, request_size);
    gpio_record_capture_active.store(false, std::memory_order_relaxed);
    microphone_level.store(0, std::memory_order_relaxed);
    if (!recorded) {
      setError("Microphone capture failed or timed out");
      continue;
    }

    if (WiFi.status() != WL_CONNECTED) {
      free(request_body);
      setError("Wi-Fi was lost during recording");
      continue;
    }

    setState(UiState::kDiscovering);
    locallink::Endpoint endpoint{};
    if (!discoverEndpoint(endpoint)) {
      free(request_body);
      setError("Speech service not discovered");
      continue;
    }
    IPAddress resolved_address;
    if (!resolveServiceHost(endpoint, resolved_address)) {
      free(request_body);
      setError("Could not resolve speech service host");
      continue;
    }
    if (WiFi.status() != WL_CONNECTED) {
      free(request_body);
      setError("Wi-Fi was lost before upload");
      continue;
    }

    setState(UiState::kUploading);
    char result[kTranscriptionCapacity] = {};
    char error[kErrorCapacity] = {};
    const bool posted = postForTranscription(
        endpoint, resolved_address, request_body, request_size, result,
        sizeof(result), error, sizeof(error));
    free(request_body);
    if (!posted) {
      setError(error[0] == '\0' ? "Transcription request failed" : error);
      continue;
    }
    if (WiFi.status() != WL_CONNECTED) {
      setError("Wi-Fi was lost while waiting for transcription");
      continue;
    }
    setState(UiState::kComplete, result);
  }
}

void startWiFi() {
  WiFi.onEvent([](arduino_event_t* event) {
    if (event->event_id == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
      Serial.printf("wifi_disconnected_reason=%u\n",
                    event->event_info.wifi_sta_disconnected.reason);
    } else if (event->event_id == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
      Serial.println("wifi_status=connected");
    }
  });
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  if (strlen(LOCALLINK_WIFI_SSID) > 0) {
    Serial.println("wifi_credentials=configured_in_firmware");
    WiFi.begin(LOCALLINK_WIFI_SSID, LOCALLINK_WIFI_PASSWORD);
  } else {
    wifi_config_t config = {};
    const esp_err_t config_result = esp_wifi_get_config(WIFI_IF_STA, &config);
    Serial.printf("wifi_credentials=%s\n",
                  config_result != ESP_OK ? "read_failed"
                  : config.sta.ssid[0] ? "saved_in_nvs" : "missing");
    WiFi.begin();  // Reuse credentials previously saved in device NVS.
  }
}

fnk0104b::MicrophoneConfig microphoneConfig() {
  return {
      LOCALLINK_PIN_I2S_MCLK,
      LOCALLINK_PIN_I2S_BCLK,
      LOCALLINK_PIN_I2S_WS,
      LOCALLINK_PIN_I2S_DATA_OUT,
      LOCALLINK_PIN_I2S_DATA_IN,
      LOCALLINK_PIN_I2C_SDA,
      LOCALLINK_PIN_I2C_SCL,
      static_cast<uint8_t>(LOCALLINK_CODEC_I2C_ADDRESS),
  };
}

}  // namespace

void setup() {
  fnk0104b::board.begin();
  fnk0104b::board.printStartupInfo("locallink-speech", kFirmwareVersion);
  pinMode(fnk0104b::pins::expansion::gpio_14, INPUT_PULLUP);
  gpio_record_button_last_reading_pressed =
      digitalRead(fnk0104b::pins::expansion::gpio_14) == LOW;
  gpio_record_button_stable_pressed = gpio_record_button_last_reading_pressed;
  Serial.printf("speech_button_gpio=%d mode=hold_to_record\n",
                fnk0104b::pins::expansion::gpio_14);
  fnk0104b::display.begin(1);
  const bool touch_ready = fnk0104b::touch.begin();
  Serial.printf("touch=%s\n", touch_ready ? "ready" : "not_found");
  initializeLvgl();

  microphone_ready = fnk0104b::microphone.begin(microphoneConfig());
  Serial.printf("microphone=%s sample_rate=%lu channels=1 bits=16\n",
                microphone_ready ? "ready" : "not_found",
                static_cast<unsigned long>(kSampleRate));
  startWiFi();

  if (xTaskCreatePinnedToCore(speechWorker, "speech-worker", 12288, nullptr, 1,
                              &speech_worker, 0) != pdPASS) {
    speech_worker = nullptr;
    setError("Could not start speech worker");
  } else if (!microphone_ready) {
    setError("ES8311 microphone initialization failed");
  } else {
    setState(UiState::kReady);
  }
  updateUi();
}

void loop() {
  pollRecordButton();
  updateUi();
  lv_timer_handler();
  delay(5);
}
