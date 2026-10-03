#if defined(ESP_PLATFORM) && !defined(ARDUINO)
#include <initializer_list>
#include <algorithm>
#include "fnk0104b/idf_microphone.hpp"
#include "fnk0104b/pins.hpp"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/i2s_std.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace fnk0104b {
namespace {
i2s_chan_handle_t input = nullptr;
i2s_chan_handle_t output = nullptr;
i2c_master_dev_handle_t touch = nullptr;
i2c_master_bus_handle_t bus = nullptr;
i2c_master_dev_handle_t codec = nullptr;
esp_err_t writeRegister(uint8_t reg, uint8_t value) {
  const uint8_t bytes[] = {reg, value};
  return i2c_master_transmit(codec, bytes, sizeof(bytes), 100);
}
esp_err_t readRegister(uint8_t reg, uint8_t& value) {
  return i2c_master_transmit_receive(codec, &reg, 1, &value, 1, 100);
}
esp_err_t configureCodec() {
  // Same verified 16 kHz / MCLK x384 coefficients as microphone.cpp.
  esp_err_t err = writeRegister(0x00, 0x1f);
  if (err != ESP_OK) return err;
  vTaskDelay(pdMS_TO_TICKS(20));
  for (const auto& entry : {0x00, 0x80}) {
    if ((err = writeRegister(0x00, entry)) != ESP_OK) return err;
  }
  if ((err = writeRegister(0x01, 0x3f)) != ESP_OK) return err;
  uint8_t reg02 = 0, reg06 = 0;
  if ((err = readRegister(0x02, reg02)) != ESP_OK ||
      (err = readRegister(0x06, reg06)) != ESP_OK) return err;
  const uint8_t registers[][2] = {
      {0x02, static_cast<uint8_t>((reg02 & 0x07) | 0x48)},
      {0x03, 0x10}, {0x04, 0x10}, {0x05, 0x00},
      {0x06, static_cast<uint8_t>((reg06 & 0xe0) | 0x03)},
      {0x07, 0x00}, {0x08, 0xff}, {0x09, 0x0c}, {0x0a, 0x0c},
      {0x0d, 0x01}, {0x0e, 0x02}, {0x12, 0x00}, {0x13, 0x10},
      {0x1c, 0x6a}, {0x37, 0x08}, {0x17, 0xc8}, {0x14, 0x1a}};
  for (const auto& entry : registers) {
    if ((err = writeRegister(entry[0], entry[1])) != ESP_OK) return err;
  }
  return ESP_OK;
}
}
static esp_err_t beginAudio(bool playback) {
  // Keep the active-low speaker amplifier disabled for the microphone test.
  ESP_ERROR_CHECK(gpio_set_direction(static_cast<gpio_num_t>(pins::audio::amplifier_enable), GPIO_MODE_OUTPUT));
  ESP_ERROR_CHECK(gpio_set_level(static_cast<gpio_num_t>(pins::audio::amplifier_enable), 1));
  i2c_master_bus_config_t bus_config = {};
  bus_config.i2c_port = I2C_NUM_0;
  bus_config.sda_io_num = static_cast<gpio_num_t>(pins::audio::i2c_sda);
  bus_config.scl_io_num = static_cast<gpio_num_t>(pins::audio::i2c_scl);
  bus_config.clk_source = I2C_CLK_SRC_DEFAULT;
  bus_config.glitch_ignore_cnt = 7;
  bus_config.flags.enable_internal_pullup = true;
  esp_err_t err = i2c_new_master_bus(&bus_config, &bus);
  if (err != ESP_OK) return err;
  i2c_device_config_t device_config = {};
  device_config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
  device_config.device_address = pins::audio::codec_i2c_address;
  device_config.scl_speed_hz = 400000;
  if ((err = i2c_master_bus_add_device(bus, &device_config, &codec)) != ESP_OK) return err;
  i2s_chan_config_t channel_config = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  channel_config.dma_desc_num = 8;
  channel_config.dma_frame_num = 256;
  if ((err = i2s_new_channel(&channel_config, playback ? &output : nullptr, &input)) != ESP_OK) return err;
  i2s_std_config_t config = {};
  config.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(16000);
  config.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_384;
  config.slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO);
  config.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;
  config.gpio_cfg.mclk = static_cast<gpio_num_t>(pins::audio::i2s_master_clock);
  config.gpio_cfg.bclk = static_cast<gpio_num_t>(pins::audio::i2s_bit_clock);
  config.gpio_cfg.ws = static_cast<gpio_num_t>(pins::audio::i2s_word_select);
  config.gpio_cfg.dout = playback ? static_cast<gpio_num_t>(pins::audio::i2s_data_out) : I2S_GPIO_UNUSED;
  config.gpio_cfg.din = static_cast<gpio_num_t>(pins::audio::i2s_data_in);
  if ((err = i2s_channel_init_std_mode(input, &config)) != ESP_OK ||
      (err = i2s_channel_enable(input)) != ESP_OK) return err;
  if (playback && (err = i2s_channel_init_std_mode(output, &config)) != ESP_OK) return err;
  if ((err = configureCodec()) != ESP_OK) return err;
  if (playback) {
    // DAC setup matches the existing speaker diagnostic. Start muted.
    if ((err = writeRegister(0x32, 179)) != ESP_OK) return err; // -6 dB
    if ((err = writeRegister(0x31, 0x60)) != ESP_OK) return err;
  }
  return ESP_OK;
}
esp_err_t beginIdfMicrophone() { return beginAudio(false); }
esp_err_t beginIdfAudio() { return beginAudio(true); }
esp_err_t setIdfSpeakerEnabled(bool enabled) {
  if (!output || !codec) return ESP_ERR_INVALID_STATE;
  if (!enabled) {
    gpio_set_level(static_cast<gpio_num_t>(pins::audio::amplifier_enable), 1);
    const esp_err_t mute = writeRegister(0x31, 0x60);
    i2s_channel_disable(output);
    return mute;
  }
  esp_err_t err = i2s_channel_enable(output);
  if (err != ESP_OK) return err;
  if ((err = writeRegister(0x31, 0x00)) != ESP_OK) {
    i2s_channel_disable(output);
    return err;
  }
  return gpio_set_level(static_cast<gpio_num_t>(pins::audio::amplifier_enable), 0);
}
esp_err_t writeIdfSpeaker(const int16_t* samples, size_t count) {
  if (!output || !samples || !count) return ESP_ERR_INVALID_ARG;
  size_t sent = 0;
  while (sent < count * sizeof(int16_t)) {
    size_t bytes = 0;
    const esp_err_t err = i2s_channel_write(output,
        reinterpret_cast<const uint8_t*>(samples) + sent,
        count * sizeof(int16_t) - sent, &bytes, 1000);
    if (err != ESP_OK) return err;
    if (!bytes) return ESP_ERR_TIMEOUT;
    sent += bytes;
  }
  return ESP_OK;
}
esp_err_t beginIdfTouch() {
  if (!bus) return ESP_ERR_INVALID_STATE;
  esp_err_t err = gpio_set_direction(static_cast<gpio_num_t>(pins::touch::reset), GPIO_MODE_OUTPUT);
  if (err != ESP_OK) return err;
  gpio_set_level(static_cast<gpio_num_t>(pins::touch::reset), 0);
  vTaskDelay(pdMS_TO_TICKS(10));
  gpio_set_level(static_cast<gpio_num_t>(pins::touch::reset), 1);
  vTaskDelay(pdMS_TO_TICKS(500));
  i2c_device_config_t config = {};
  config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
  config.device_address = pins::touch::i2c_address;
  config.scl_speed_hz = 400000;
  if ((err = i2c_master_probe(bus, config.device_address, 100)) != ESP_OK) return err;
  return i2c_master_bus_add_device(bus, &config, &touch);
}
esp_err_t readIdfTouch(IdfTouchPoint& point) {
  if (!touch) return ESP_ERR_INVALID_STATE;
  uint8_t values[5] = {};
  // Use the same verified registers as board.cpp. The controller's I2C
  // timing supports repeated START; no 10 ms software wait is required.
  // Read registers individually rather than assume burst auto-increment.
  for (unsigned i = 0; i < sizeof(values); ++i) {
    const uint8_t reg = 0x02 + i;
    const esp_err_t err = i2c_master_transmit_receive(touch, &reg, 1, values + i, 1, 20);
    if (err != ESP_OK) return err;
    if (i == 0 && (values[0] & 15) == 0) { point.pressed = false; return ESP_OK; }
  }
  const int raw_x = ((values[1] & 15) << 8) | values[2];
  const int raw_y = ((values[3] & 15) << 8) | values[4];
  point = {std::clamp(raw_y, 0, 319), std::clamp(240 - raw_x, 0, 239), true};
  return ESP_OK;
}
esp_err_t readIdfMicrophone(int16_t* samples, size_t count) {
  if (!input || !samples || !count) return ESP_ERR_INVALID_ARG;
  size_t received = 0;
  const size_t total = count * sizeof(int16_t);
  while (received < total) {
    size_t bytes = 0;
    const esp_err_t err = i2s_channel_read(input, reinterpret_cast<uint8_t*>(samples) + received,
                                        total - received, &bytes, 1000);
    if (err != ESP_OK) return err;
    if (!bytes) return ESP_ERR_TIMEOUT;
    received += bytes;
  }
  return ESP_OK;
}
}
#endif
