#include <Arduino.h>

#include <fnk0104b/board.hpp>

namespace {

struct Pattern {
  uint16_t color;
  const char* name;
};

constexpr Pattern kPatterns[] = {
    {TFT_RED, "RED"},
    {TFT_GREEN, "GREEN"},
    {TFT_BLUE, "BLUE"},
    {TFT_WHITE, "WHITE"},
    {TFT_BLACK, "BLACK"},
};
constexpr size_t kPatternCount = sizeof(kPatterns) / sizeof(kPatterns[0]);
size_t pattern_index = 0;
uint32_t next_pattern_at = 0;

void showPattern(const Pattern& pattern) {
  TFT_eSPI& tft = fnk0104b::display.driver();
  tft.fillScreen(pattern.color);
  tft.setTextColor(pattern.color == TFT_WHITE ? TFT_BLACK : TFT_WHITE,
                   pattern.color);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("FNK0104B DISPLAY", tft.width() / 2, tft.height() / 2 - 12, 2);
  tft.drawString(pattern.name, tft.width() / 2, tft.height() / 2 + 12, 4);
}

}  // namespace

void setup() {
  fnk0104b::board.begin();
  fnk0104b::board.printStartupInfo("display-diagnostic", "0.2.0");
  fnk0104b::display.begin(1);
  Serial.printf("display_width=%d display_height=%d\n",
                fnk0104b::display.driver().width(),
                fnk0104b::display.driver().height());
  showPattern(kPatterns[pattern_index]);
  next_pattern_at = millis() + 1400;
}

void loop() {
  if (static_cast<int32_t>(millis() - next_pattern_at) >= 0) {
    pattern_index = (pattern_index + 1) % kPatternCount;
    showPattern(kPatterns[pattern_index]);
    Serial.printf("display_pattern=%s\n", kPatterns[pattern_index].name);
    next_pattern_at = millis() + 1400;
  }
}
