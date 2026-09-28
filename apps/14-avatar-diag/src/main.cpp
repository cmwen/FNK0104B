#include <Arduino.h>

#include <fnk0104b/board.hpp>
#include <ui/avatar_assets.hpp>

namespace {

constexpr int kScale = 3;
constexpr int kAvatarPixels = 32;
constexpr int kDrawSize = kAvatarPixels * kScale;
constexpr int kCellWidth = 120;
constexpr int kCellHeight = 160;
constexpr int kAvatarInsetX = 12;
constexpr int kAvatarInsetY = 36;
constexpr uint32_t kFramePeriodMs = 100;
constexpr uint32_t kStatePeriodMs = 8000;
constexpr uint32_t kReportPeriodMs = 5000;
constexpr uint16_t kBackground = 0x1082;

struct Agent {
  const char* name;
  uint32_t identity;
};

Agent agents[] = {
    {"Ada", 0}, {"Bex", 0}, {"Cora", 0}, {"Dexter", 0},
};

constexpr const char* kMoodNames[] = {"IDLE", "THINKING", "INPUT", "ERROR"};
constexpr uint16_t kMoodColors[] = {TFT_GREEN, TFT_CYAN, TFT_ORANGE, TFT_RED};

ui::avatar::Canvas canvas;
uint16_t scaled_frame[kDrawSize * kDrawSize];
uint32_t next_frame_at = 0;
uint32_t next_state_at = 0;
uint32_t next_report_at = 0;
uint32_t frame_count = 0;
uint64_t compose_total_us = 0;
uint64_t transfer_total_us = 0;
uint32_t max_frame_us = 0;
uint8_t state_offset = 0;

ui::avatar::Mood moodFor(size_t index) {
  return static_cast<ui::avatar::Mood>((index + state_offset) % 4);
}

void scaleFrame() {
  for (int source_y = 0; source_y < kAvatarPixels; ++source_y) {
    for (int repeat_y = 0; repeat_y < kScale; ++repeat_y) {
      uint16_t* target = scaled_frame +
                         (source_y * kScale + repeat_y) * kDrawSize;
      for (int source_x = 0; source_x < kAvatarPixels; ++source_x) {
        const uint16_t pixel =
            canvas.pixels[source_y * kAvatarPixels + source_x];
        for (int repeat_x = 0; repeat_x < kScale; ++repeat_x) {
          target[source_x * kScale + repeat_x] = pixel;
        }
      }
    }
  }
}

void drawLabels() {
  TFT_eSPI& tft = fnk0104b::display.driver();
  tft.setTextDatum(TL_DATUM);
  tft.setTextSize(1);
  for (size_t i = 0; i < 4; ++i) {
    const int column = static_cast<int>(i % 2);
    const int row = static_cast<int>(i / 2);
    const int x = column * kCellWidth + kAvatarInsetX;
    const int name_y = row * kCellHeight + 18;
    const int badge_y = row * kCellHeight + 134;
    tft.fillRect(x, name_y, kDrawSize, 16, kBackground);
    tft.setTextColor(TFT_WHITE, kBackground);
    tft.drawString(agents[i].name, x, name_y, 2);
    const uint8_t mood = (i + state_offset) % 4;
    const uint16_t color = kMoodColors[mood];
    tft.fillRoundRect(x, badge_y, kDrawSize, 22, 4, color);
    tft.setTextColor(mood == 3 ? TFT_WHITE : TFT_BLACK, color);
    tft.drawRightString(kMoodNames[mood], x + kDrawSize - 4,
                        badge_y + 3, 2);
    const int icon_x = x + 3;
    const int icon_y = badge_y + 1;
    if (mood == 0) {
      tft.drawLine(icon_x + 2, icon_y + 10, icon_x + 7, icon_y + 15,
                   TFT_BLACK);
      tft.drawLine(icon_x + 7, icon_y + 15, icon_x + 17, icon_y + 4,
                   TFT_BLACK);
    } else if (mood == 1) {
      for (int dot = 0; dot < 3; ++dot) {
        tft.fillCircle(icon_x + 4 + dot * 6, icon_y + 11, 2, TFT_BLACK);
      }
    } else {
      tft.setTextColor(mood == 3 ? TFT_WHITE : TFT_BLACK, color);
      tft.drawString(mood == 2 ? "?" : "!", icon_x + 5, icon_y + 2, 2);
    }
  }
}

void drawColorCheck() {
  TFT_eSPI& tft = fnk0104b::display.driver();
  constexpr uint16_t colors[] = {TFT_RED, TFT_GREEN, TFT_BLUE};
  uint16_t pixels[8 * 8];
  for (int i = 0; i < 3; ++i) {
    for (uint16_t& pixel : pixels) pixel = colors[i];
    const int x = 128 + i * 36;
    tft.fillRect(x, 3, 8, 8, colors[i]);
    tft.pushImage(x + 10, 3, 8, 8, pixels);
  }
  Serial.println("color_check=top_right RGB pairs: left fillRect, right pushImage");
}

void drawFrame(uint32_t tick) {
  TFT_eSPI& tft = fnk0104b::display.driver();
  const uint32_t started_at = micros();
  uint32_t compose_us = 0;
  uint32_t transfer_us = 0;
  for (size_t i = 0; i < 4; ++i) {
    const uint32_t compose_started = micros();
    ui::avatar::render(canvas, agents[i].identity, moodFor(i), tick);
    scaleFrame();
    compose_us += micros() - compose_started;

    const int x = static_cast<int>(i % 2) * kCellWidth + kAvatarInsetX;
    const int y = static_cast<int>(i / 2) * kCellHeight + kAvatarInsetY;
    const uint32_t transfer_started = micros();
    tft.pushImage(x, y, kDrawSize, kDrawSize, scaled_frame);
    transfer_us += micros() - transfer_started;
  }
  compose_total_us += compose_us;
  transfer_total_us += transfer_us;
  const uint32_t elapsed = micros() - started_at;
  if (elapsed > max_frame_us) max_frame_us = elapsed;
  ++frame_count;
}

void printReport() {
  if (frame_count == 0) return;
  Serial.printf(
      "avatar_frames=%lu avg_compose_us=%lu avg_transfer_us=%lu "
      "max_frame_us=%lu free_heap_bytes=%u free_psram_bytes=%u\n",
      static_cast<unsigned long>(frame_count),
      static_cast<unsigned long>(compose_total_us / frame_count),
      static_cast<unsigned long>(transfer_total_us / frame_count),
      static_cast<unsigned long>(max_frame_us),
      static_cast<unsigned>(ESP.getFreeHeap()),
      static_cast<unsigned>(ESP.getFreePsram()));
  frame_count = 0;
  compose_total_us = 0;
  transfer_total_us = 0;
  max_frame_us = 0;
}

}  // namespace

void setup() {
  fnk0104b::board.begin();
  fnk0104b::board.printStartupInfo("avatar-diag", "0.1.0");
  fnk0104b::display.begin(0);
  TFT_eSPI& tft = fnk0104b::display.driver();
  tft.fillScreen(kBackground);
  // RGB565 uint16_t values are little-endian in RAM. TFT_eSPI must swap their
  // bytes before sending them as high-byte-first display pixels.
  tft.setSwapBytes(true);
  tft.setTextColor(TFT_CYAN, kBackground);
  tft.setTextDatum(TL_DATUM);
  tft.drawString("AVATAR DIAG", 8, 0, 2);
  drawColorCheck();

  for (Agent& agent : agents) {
    agent.identity = ui::avatar::hashId(agent.name);
    Serial.printf("avatar_id=%s hash=%08lx\n", agent.name,
                  static_cast<unsigned long>(agent.identity));
  }
  Serial.printf("avatar_canvas_bytes=%u scaled_buffer_bytes=%u display=%dx%d\n",
                static_cast<unsigned>(sizeof(canvas)),
                static_cast<unsigned>(sizeof(scaled_frame)), tft.width(),
                tft.height());
  drawLabels();
  drawFrame(0);
  const uint32_t now = millis();
  next_frame_at = now + kFramePeriodMs;
  next_state_at = now + kStatePeriodMs;
  next_report_at = now + kReportPeriodMs;
}

void loop() {
  const uint32_t now = millis();
  if (static_cast<int32_t>(now - next_state_at) >= 0) {
    state_offset = (state_offset + 1) % 4;
    drawLabels();
    next_state_at = now + kStatePeriodMs;
  }
  if (static_cast<int32_t>(now - next_frame_at) >= 0) {
    drawFrame(now / kFramePeriodMs);
    next_frame_at = now + kFramePeriodMs;
  }
  if (static_cast<int32_t>(now - next_report_at) >= 0) {
    printReport();
    next_report_at = now + kReportPeriodMs;
  }
  delay(1);
}
