#pragma once
#include <stdint.h>
#include "esp_err.h"

namespace fnk0104b {
// Landscape ILI9341 display. Only one rendering task may own these calls.
constexpr int kIdfDisplayWidth = 320;
constexpr int kIdfDisplayHeight = 240;
esp_err_t beginIdfDisplay();
void idfDisplayFill(int x, int y, int width, int height, uint16_t color);
void idfDisplayText(int x, int y, const char* text, uint16_t color, int scale = 1);
esp_err_t flushIdfDisplay();
}
