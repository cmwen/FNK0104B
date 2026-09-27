#include <Arduino.h>
#include <Wire.h>

#include <driver/i2s.h>
#include <esp_intr_alloc.h>

#include "fnk0104b/board.hpp"

namespace fnk0104b {
namespace {

constexpr i2s_port_t kI2sPort = I2S_NUM_0;
constexpr uint32_t kSampleRate = 16000;
constexpr uint32_t kMclkMultiple = 384;
constexpr uint32_t kMclkHz = kSampleRate * kMclkMultiple;
constexpr uint32_t kI2cClockHz = 400000;

bool writeCodecRegister(uint8_t device_address, uint8_t reg, uint8_t value) {
  Wire.beginTransmission(device_address);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

bool readCodecRegister(uint8_t address, uint8_t reg, uint8_t& value) {
  Wire.beginTransmission(address);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0 ||
      Wire.requestFrom(address, static_cast<uint8_t>(1)) != 1 ||
      !Wire.available()) {
    return false;
  }
  value = static_cast<uint8_t>(Wire.read());
  return true;
}

bool initializeEs8311(uint8_t address) {
  // ES8311 is configured as an I2S slave using the board's 6.144 MHz MCLK.
  // These values are the 16 kHz / 16-bit microphone path from Espressif's
  // Apache-2.0 ES8311 driver; the mic is analog, not PDM.
  if (!writeCodecRegister(address, 0x00, 0x1F)) return false;
  delay(20);
  if (!writeCodecRegister(address, 0x00, 0x00) ||
      !writeCodecRegister(address, 0x00, 0x80) ||
      !writeCodecRegister(address, 0x01, 0x3F)) {
    return false;
  }

  uint8_t reg02 = 0;
  uint8_t reg06 = 0;
  if (!readCodecRegister(address, 0x02, reg02) ||
      !readCodecRegister(address, 0x06, reg06)) {
    return false;
  }

  // 6.144 MHz MCLK / 16 kHz, 16-bit Philips I2S clock coefficients.
  const uint8_t configured_reg02 = (reg02 & 0x07) | 0x48;
  const uint8_t configured_reg06 = (reg06 & 0xE0) | 0x03;
  const uint8_t registers[][2] = {
      {0x02, configured_reg02}, {0x03, 0x10}, {0x04, 0x10}, {0x05, 0x00},
      {0x06, configured_reg06}, {0x07, 0x00}, {0x08, 0xFF}, {0x09, 0x0C},
      {0x0A, 0x0C},            {0x0D, 0x01}, {0x0E, 0x02}, {0x12, 0x00},
      {0x13, 0x10},            {0x1C, 0x6A}, {0x37, 0x08}, {0x17, 0xC8},
      {0x14, 0x1A},
  };
  for (const auto& entry : registers) {
    if (!writeCodecRegister(address, entry[0], entry[1])) return false;
  }
  return true;
}

}  // namespace

MicrophoneSupport microphone;

bool MicrophoneSupport::begin(const MicrophoneConfig& config) {
  ready_ = false;

  if (!Wire.begin(config.i2c_sda_pin, config.i2c_scl_pin, kI2cClockHz)) {
    return false;
  }
  Wire.setClock(kI2cClockHz);
  Wire.setTimeOut(50);

  i2s_config_t i2s_config = {};
  i2s_config.mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_RX);
  i2s_config.sample_rate = kSampleRate;
  i2s_config.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  i2s_config.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
  i2s_config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  i2s_config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  i2s_config.dma_buf_count = 8;
  i2s_config.dma_buf_len = 256;
  i2s_config.use_apll = true;
  i2s_config.fixed_mclk = kMclkHz;
  i2s_config.mclk_multiple = static_cast<i2s_mclk_multiple_t>(kMclkMultiple);

  if (i2s_driver_install(kI2sPort, &i2s_config, 0, nullptr) != ESP_OK) {
    return false;
  }

  const i2s_pin_config_t i2s_pins = {
      config.mclk_pin,
      config.bclk_pin,
      config.word_select_pin,
      config.data_out_pin,
      config.data_in_pin,
  };
  if (i2s_set_pin(kI2sPort, &i2s_pins) != ESP_OK ||
      !initializeEs8311(config.codec_i2c_address)) {
    i2s_driver_uninstall(kI2sPort);
    return false;
  }

  i2s_zero_dma_buffer(kI2sPort);
  ready_ = true;
  return true;
}

bool MicrophoneSupport::capture(int16_t* samples, size_t requested_samples,
                                size_t& captured_samples,
                                uint32_t timeout_ms,
                                MicrophoneStopCheck should_stop,
                                void* stop_context) {
  captured_samples = 0;
  if (!ready_ || samples == nullptr || requested_samples == 0) return false;

  const uint32_t started = millis();
  while (captured_samples < requested_samples &&
         static_cast<uint32_t>(millis() - started) < timeout_ms &&
         (should_stop == nullptr || !should_stop(stop_context))) {
    size_t bytes_read = 0;
    const size_t remaining_bytes =
        (requested_samples - captured_samples) * sizeof(int16_t);
    const esp_err_t error = i2s_read(
        kI2sPort, samples + captured_samples, remaining_bytes, &bytes_read,
        pdMS_TO_TICKS(100));
    if (error != ESP_OK) return false;
    captured_samples += bytes_read / sizeof(int16_t);
  }
  return captured_samples > 0;
}

bool MicrophoneSupport::ready() const { return ready_; }

}  // namespace fnk0104b
