#if defined(ESP_PLATFORM) && !defined(ARDUINO)
#include <algorithm>
#include <cstring>
#include "fnk0104b/idf_display.hpp"
#include "fnk0104b/pins.hpp"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_heap_caps.h"
#include "esp_lcd_ili9341.h"
#include "esp_lcd_io_spi.h"
#include "esp_lcd_panel_ops.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

namespace fnk0104b {
namespace {
esp_lcd_panel_handle_t panel = nullptr;
SemaphoreHandle_t completed = nullptr;
uint16_t* frame = nullptr;
uint16_t* stripe = nullptr;
constexpr int kStripeRows = 16;
// Original compact 5x7 uppercase diagnostic font, one five-bit row per byte.
constexpr uint8_t kLetters[][7] = {
 {14,17,17,31,17,17,17},{30,17,17,30,17,17,30},{14,17,16,16,16,17,14},
 {30,17,17,17,17,17,30},{31,16,16,30,16,16,31},{31,16,16,30,16,16,16},
 {14,17,16,23,17,17,15},{17,17,17,31,17,17,17},{14,4,4,4,4,4,14},
 {7,2,2,2,18,18,12},{17,18,20,24,20,18,17},{16,16,16,16,16,16,31},
 {17,27,21,21,17,17,17},{17,25,21,19,17,17,17},{14,17,17,17,17,17,14},
 {30,17,17,30,16,16,16},{14,17,17,17,21,18,13},{30,17,17,30,20,18,17},
 {15,16,16,14,1,1,30},{31,4,4,4,4,4,4},{17,17,17,17,17,17,14},
 {17,17,17,17,17,10,4},{17,17,17,21,21,21,10},{17,17,10,4,10,17,17},
 {17,17,10,4,4,4,4},{31,1,2,4,8,16,31}};
constexpr uint8_t kDigits[][7] = {
 {14,17,19,21,25,17,14},{4,12,4,4,4,4,14},{14,17,1,2,4,8,31},
 {30,1,1,14,1,1,30},{2,6,10,18,31,2,2},{31,16,16,30,1,1,30},
 {14,16,16,30,17,17,14},{31,1,2,4,8,8,8},{14,17,17,14,17,17,14},
 {14,17,17,15,1,1,14}};
uint8_t rowBits(char ch, int row) {
  if (ch >= 'a' && ch <= 'z') ch -= 'a' - 'A';
  if (ch >= 'A' && ch <= 'Z') return kLetters[ch - 'A'][row];
  if (ch >= '0' && ch <= '9') return kDigits[ch - '0'][row];
  switch (ch) {
    case '.': return row == 6 ? 4 : 0;
    case ':': return row == 2 || row == 5 ? 4 : 0;
    case '-': return row == 3 ? 14 : 0;
    case '"': return row < 2 ? 10 : 0;
    case '/': return 1 << std::min(4, 6 - row);
    case '%': return row == 0 || row == 6 ? 17 : 1 << std::min(4, 6 - row);
    default: return 0;
  }
}
bool transferDone(esp_lcd_panel_io_handle_t, esp_lcd_panel_io_event_data_t*, void*) {
  BaseType_t awakened = pdFALSE;
  xSemaphoreGiveFromISR(completed, &awakened);
  return awakened == pdTRUE;
}
}
esp_err_t beginIdfDisplay() {
  esp_err_t err;
  const auto backlight = static_cast<gpio_num_t>(pins::display::backlight);
  if ((err = gpio_set_direction(backlight, GPIO_MODE_OUTPUT)) != ESP_OK ||
      (err = gpio_set_level(backlight, 0)) != ESP_OK) return err;
  frame = static_cast<uint16_t*>(heap_caps_calloc(kIdfDisplayWidth * kIdfDisplayHeight,
                                                sizeof(uint16_t), MALLOC_CAP_SPIRAM));
  stripe = static_cast<uint16_t*>(heap_caps_malloc(kIdfDisplayWidth * kStripeRows * sizeof(uint16_t),
                                                 MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL));
  completed = xSemaphoreCreateBinary();
  if (!frame || !stripe || !completed) return ESP_ERR_NO_MEM;
  spi_bus_config_t bus = {};
  bus.mosi_io_num = pins::display::mosi;
  bus.miso_io_num = pins::display::miso;
  bus.sclk_io_num = pins::display::clock;
  bus.quadwp_io_num = -1;
  bus.quadhd_io_num = -1;
  bus.max_transfer_sz = kIdfDisplayWidth * kStripeRows * sizeof(uint16_t);
  if ((err = spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO)) != ESP_OK) return err;
  esp_lcd_panel_io_handle_t io = nullptr;
  esp_lcd_panel_io_spi_config_t io_config = {};
  io_config.cs_gpio_num = static_cast<gpio_num_t>(pins::display::chip_select);
  io_config.dc_gpio_num = static_cast<gpio_num_t>(pins::display::data_command);
  io_config.spi_mode = 0;
  io_config.pclk_hz = 27000000;  // Match existing TFT_eSPI diagnostic setup.
  io_config.trans_queue_depth = 1;
  io_config.on_color_trans_done = transferDone;
  io_config.lcd_cmd_bits = 8;
  io_config.lcd_param_bits = 8;
  if ((err = esp_lcd_new_panel_io_spi(SPI2_HOST, &io_config, &io)) != ESP_OK) return err;
  esp_lcd_panel_dev_config_t panel_config = {};
  panel_config.reset_gpio_num = GPIO_NUM_NC;  // Display reset shares CHIP_PU.
  panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR;
  panel_config.bits_per_pixel = 16;
  if ((err = esp_lcd_new_panel_ili9341(io, &panel_config, &panel)) != ESP_OK ||
      (err = esp_lcd_panel_reset(panel)) != ESP_OK ||
      (err = esp_lcd_panel_init(panel)) != ESP_OK ||
      (err = esp_lcd_panel_invert_color(panel, true)) != ESP_OK ||
      (err = esp_lcd_panel_swap_xy(panel, true)) != ESP_OK ||
      (err = esp_lcd_panel_mirror(panel, false, false)) != ESP_OK ||
      (err = esp_lcd_panel_disp_on_off(panel, true)) != ESP_OK ||
      (err = flushIdfDisplay()) != ESP_OK) return err;
  return gpio_set_level(backlight, 1);
}
void idfDisplayFill(int x, int y, int width, int height, uint16_t color) {
  if (!frame) return;
  // Store in wire order: RGB565 bytes are sent MSB first to the panel.
  const uint16_t wire = (color >> 8) | (color << 8);
  for (int row = std::max(0, y); row < std::min(kIdfDisplayHeight, y + height); ++row) {
    for (int col = std::max(0, x); col < std::min(kIdfDisplayWidth, x + width); ++col) {
      frame[row * kIdfDisplayWidth + col] = wire;
    }
  }
}
void idfDisplayText(int x, int y, const char* text, uint16_t color, int scale) {
  if (!text || scale <= 0) return;
  for (; *text && x < kIdfDisplayWidth; ++text, x += 6 * scale) {
    for (int row = 0; row < 7; ++row) {
      const uint8_t bits = rowBits(*text, row);
      for (int col = 0; col < 5; ++col) {
        if (bits & (1 << (4 - col))) idfDisplayFill(x + col * scale, y + row * scale, scale, scale, color);
      }
    }
  }
}
esp_err_t flushIdfDisplayRows(int start, int height) {
  if (start < 0 || height <= 0 || start + height > kIdfDisplayHeight) return ESP_ERR_INVALID_ARG;
  if (!panel || !frame || !stripe) return ESP_ERR_INVALID_STATE;
  for (int y = start; y < start + height; y += kStripeRows) {
    const int rows = std::min(kStripeRows, start + height - y);
    std::memcpy(stripe, frame + y * kIdfDisplayWidth, rows * kIdfDisplayWidth * sizeof(uint16_t));
    const esp_err_t err = esp_lcd_panel_draw_bitmap(panel, 0, y, kIdfDisplayWidth, y + rows, stripe);
    if (err != ESP_OK) return err;
    if (xSemaphoreTake(completed, pdMS_TO_TICKS(1000)) != pdTRUE) return ESP_ERR_TIMEOUT;
  }
  return ESP_OK;
}
esp_err_t flushIdfDisplay() { return flushIdfDisplayRows(0, kIdfDisplayHeight); }
}
#endif
