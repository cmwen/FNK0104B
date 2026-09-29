#include <Arduino.h>

#include <fnk0104b/board.hpp>
#include <fnk0104b/pins.hpp>

namespace {

constexpr char kFirmwareVersion[] = "0.2.0";
constexpr int16_t kScreenWidth = fnk0104b::pins::display::native_height;
constexpr int16_t kKeyboardTop = 54;
constexpr int16_t kKeyboardBottom = 230;
constexpr int16_t kBlackKeyBottom = 153;
constexpr int16_t kVolumeTop = 25;
constexpr int16_t kVolumeBottom = 49;
constexpr int16_t kVolumeSliderLeft = 91;
constexpr int16_t kVolumeSliderRight = 309;
constexpr int16_t kVolumeSliderCenterY = 37;

struct PianoKey {
  const char* name;
  uint16_t frequency_hz;
};

constexpr PianoKey kKeys[] = {
    {"C4", 262}, {"C#4", 277}, {"D4", 294}, {"D#4", 311},
    {"E4", 330}, {"F4", 349}, {"F#4", 370}, {"G4", 392},
    {"G#4", 415}, {"A4", 440}, {"A#4", 466}, {"B4", 494},
    {"C5", 523},
};
constexpr uint8_t kWhiteKeyIndexes[] = {0, 2, 4, 5, 7, 9, 11, 12};
constexpr uint8_t kBlackKeyIndexes[] = {1, 3, 6, 8, 10};
constexpr uint8_t kBlackKeyBoundaries[] = {1, 2, 4, 5, 6};

bool display_ready = false;
bool touch_ready = false;
bool speaker_ready = false;
int8_t active_key = -1;
uint8_t volume_percent = 85;

TFT_eSPI& screen() { return fnk0104b::display.driver(); }

void drawWhiteKey(uint8_t key_index, bool pressed) {
  const int16_t white_count = sizeof(kWhiteKeyIndexes) / sizeof(kWhiteKeyIndexes[0]);
  for (int16_t i = 0; i < white_count; ++i) {
    if (kWhiteKeyIndexes[i] != key_index) continue;
    const int16_t left = i * kScreenWidth / white_count;
    const int16_t right = (i + 1) * kScreenWidth / white_count;
    const uint16_t fill = pressed ? TFT_YELLOW : TFT_WHITE;
    screen().fillRect(left, kKeyboardTop, right - left, kKeyboardBottom - kKeyboardTop,
                      fill);
    screen().drawRect(left, kKeyboardTop, right - left,
                      kKeyboardBottom - kKeyboardTop, TFT_BLACK);
    screen().setTextColor(TFT_BLACK, fill);
    screen().setTextDatum(BC_DATUM);
    screen().drawString(kKeys[key_index].name,
                        left + (right - left) / 2,
                        kKeyboardBottom - 8, 2);
    break;
  }
}

void drawBlackKey(uint8_t key_index, bool pressed) {
  const int16_t black_width = kScreenWidth / 18;
  for (uint8_t i = 0; i < sizeof(kBlackKeyIndexes); ++i) {
    if (kBlackKeyIndexes[i] != key_index) continue;
    const int16_t white_count = sizeof(kWhiteKeyIndexes) / sizeof(kWhiteKeyIndexes[0]);
    const int16_t boundary =
        kBlackKeyBoundaries[i] * kScreenWidth / white_count;
    const int16_t left = boundary - black_width / 2;
    const uint16_t fill = pressed ? TFT_ORANGE : TFT_BLACK;
    screen().fillRoundRect(left, kKeyboardTop, black_width, kBlackKeyBottom - kKeyboardTop,
                           3, fill);
    screen().drawRoundRect(left, kKeyboardTop, black_width,
                           kBlackKeyBottom - kKeyboardTop, 3, TFT_DARKGREY);
    screen().setTextColor(TFT_WHITE, fill);
    screen().setTextDatum(BC_DATUM);
    screen().drawString(kKeys[key_index].name,
                        left + black_width / 2, kBlackKeyBottom - 8, 1);
    break;
  }
}

void drawKey(uint8_t key_index, bool pressed) {
  bool is_white = false;
  for (uint8_t index : kWhiteKeyIndexes) {
    if (index == key_index) is_white = true;
  }
  if (is_white) {
    drawWhiteKey(key_index, pressed);
    // A white-key redraw covers the black-key overhangs; restore those small
    // overlays without repainting the rest of the display.
    for (uint8_t index : kBlackKeyIndexes) {
      drawBlackKey(index, index == active_key);
    }
  } else {
    drawBlackKey(key_index, pressed);
  }
}

void drawStatus(int8_t pressed_key) {
  screen().fillRect(0, 231, kScreenWidth, 9, TFT_DARKGREY);
  if (!speaker_ready) {
    screen().setTextColor(TFT_RED, TFT_DARKGREY);
    screen().setTextDatum(TL_DATUM);
    screen().drawString("Speaker init failed", 8, 232, 1);
  } else if (!touch_ready) {
    screen().setTextColor(TFT_RED, TFT_DARKGREY);
    screen().setTextDatum(TL_DATUM);
    screen().drawString("Touch init failed", 8, 232, 1);
  } else if (pressed_key >= 0) {
    screen().setTextColor(TFT_WHITE, TFT_DARKGREY);
    screen().setTextDatum(TL_DATUM);
    screen().drawString(String("Playing ") + kKeys[pressed_key].name, 8, 232, 1);
  } else {
    screen().setTextColor(TFT_LIGHTGREY, TFT_DARKGREY);
    screen().setTextDatum(TL_DATUM);
    screen().drawString("Release to silence", 8, 232, 1);
  }
}

void drawVolumeSlider() {
  screen().fillRect(0, kVolumeTop, kScreenWidth,
                    kVolumeBottom - kVolumeTop + 1, TFT_NAVY);
  screen().setTextColor(TFT_WHITE, TFT_NAVY);
  screen().setTextDatum(TL_DATUM);
  screen().drawString(String("VOL ") + volume_percent + "%", 8, 30, 1);

  const int16_t track_width = kVolumeSliderRight - kVolumeSliderLeft;
  const int16_t filled_width = track_width * volume_percent / 100;
  screen().fillRoundRect(kVolumeSliderLeft, kVolumeSliderCenterY - 4,
                         track_width, 8, 4, TFT_DARKGREY);
  if (filled_width > 0) {
    screen().fillRoundRect(kVolumeSliderLeft, kVolumeSliderCenterY - 4,
                           filled_width, 8, 4, TFT_CYAN);
  }
  const int16_t knob_x = kVolumeSliderLeft + filled_width;
  screen().fillCircle(knob_x, kVolumeSliderCenterY, 7, TFT_WHITE);
  screen().drawCircle(knob_x, kVolumeSliderCenterY, 7, TFT_BLACK);
}

void drawKeyboard() {
  screen().fillScreen(TFT_DARKGREY);
  screen().fillRect(0, 0, kScreenWidth, kVolumeBottom + 1, TFT_NAVY);
  screen().setTextColor(TFT_WHITE, TFT_NAVY);
  screen().setTextDatum(TL_DATUM);
  screen().drawString("Speaker piano", 8, 5, 2);
  drawVolumeSlider();

  for (uint8_t key_index : kWhiteKeyIndexes) drawWhiteKey(key_index, false);
  for (uint8_t key_index : kBlackKeyIndexes) drawBlackKey(key_index, false);
  drawStatus(-1);
}

int8_t keyAt(int16_t x, int16_t y) {
  if (x < 0 || x >= kScreenWidth || y < kKeyboardTop || y >= kKeyboardBottom) {
    return -1;
  }

  if (y < kBlackKeyBottom) {
    const int16_t white_count = sizeof(kWhiteKeyIndexes) / sizeof(kWhiteKeyIndexes[0]);
    const int16_t black_width = kScreenWidth / 18;
    for (uint8_t i = 0; i < sizeof(kBlackKeyIndexes); ++i) {
      const int16_t boundary =
          kBlackKeyBoundaries[i] * kScreenWidth / white_count;
      if (abs(x - boundary) <= black_width / 2) return kBlackKeyIndexes[i];
    }
  }

  const int16_t white_count = sizeof(kWhiteKeyIndexes) / sizeof(kWhiteKeyIndexes[0]);
  const int16_t white_index = x * white_count / kScreenWidth;
  return kWhiteKeyIndexes[white_index];
}

bool isVolumeSliderTouch(int16_t x, int16_t y) {
  return y >= kVolumeTop && y <= kVolumeBottom &&
         x >= kVolumeSliderLeft - 7 && x <= kVolumeSliderRight + 7;
}

uint8_t volumeAt(int16_t x) {
  const int16_t clamped_x = constrain(x, kVolumeSliderLeft, kVolumeSliderRight);
  const int16_t track_width = kVolumeSliderRight - kVolumeSliderLeft;
  return static_cast<uint8_t>(
      ((clamped_x - kVolumeSliderLeft) * 100 + track_width / 2) / track_width);
}

}  // namespace

void setup() {
  fnk0104b::board.begin();
  fnk0104b::board.printStartupInfo("speaker-keyboard", kFirmwareVersion);

  fnk0104b::display.begin(1);
  display_ready = true;
  touch_ready = fnk0104b::touch.begin();
  speaker_ready = fnk0104b::speaker.begin();
  if (speaker_ready) {
    speaker_ready = fnk0104b::speaker.setVolume(volume_percent);
  }

  Serial.printf("display=%s touch=%s speaker=%s\n",
                display_ready ? "ready" : "not_ready",
                touch_ready ? "ready" : "not_ready",
                speaker_ready ? "ready" : "not_ready");
  drawKeyboard();
}

void loop() {
  if (!display_ready || !touch_ready || !speaker_ready) {
    delay(1000);
    return;
  }

  fnk0104b::TouchPoint point{};
  if (!fnk0104b::touch.read(point)) {
    delay(10);
    return;
  }

  const int8_t next_key = point.pressed ? keyAt(point.x, point.y) : -1;
  if (next_key != active_key) {
    const int8_t previous_key = active_key;
    active_key = next_key;
    const uint16_t frequency =
        active_key >= 0 ? kKeys[active_key].frequency_hz : 0;
    fnk0104b::speaker.setTone(frequency);
    if (active_key >= 0) {
      Serial.printf("speaker_note=%s frequency_hz=%u\n",
                    kKeys[active_key].name, frequency);
    } else {
      Serial.println("speaker_note=silence");
    }
    if (previous_key >= 0) drawKey(previous_key, false);
    if (active_key >= 0) drawKey(active_key, true);
    drawStatus(active_key);
  }

  if (point.pressed && isVolumeSliderTouch(point.x, point.y)) {
    const uint8_t next_volume = volumeAt(point.x);
    if (next_volume != volume_percent) {
      if (fnk0104b::speaker.setVolume(next_volume)) {
        volume_percent = next_volume;
        Serial.printf("speaker_volume_pct=%u\n", volume_percent);
        drawVolumeSlider();
      } else {
        Serial.println("speaker_volume=write_failed");
      }
    }
  }
  delay(5);
}
