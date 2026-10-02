#include <Arduino.h>
#include <Wire.h>

#include <math.h>

#include <driver/i2s.h>
#include <esp_intr_alloc.h>

#include "fnk0104b/board.hpp"
#include "fnk0104b/pins.hpp"

namespace fnk0104b {
namespace {

constexpr i2s_port_t kI2sPort = I2S_NUM_0;
constexpr uint32_t kSampleRate = 16000;
constexpr uint32_t kMclkMultiple = 384;
constexpr uint32_t kMclkHz = kSampleRate * kMclkMultiple;
constexpr uint32_t kI2cClockHz = 400000;
constexpr size_t kSamplesPerChunk = 256;
constexpr int16_t kToneAmplitude = 12000;
constexpr float kMinimumAudibleVolumeDb = -40.0f;

bool writeCodecRegister(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(pins::audio::codec_i2c_address);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

bool readCodecRegister(uint8_t reg, uint8_t& value) {
  const uint8_t address =
      static_cast<uint8_t>(pins::audio::codec_i2c_address);
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

uint8_t codecVolumeRegister(uint8_t percent) {
  const float linear_gain = static_cast<float>(percent) / 100.0f;
  float gain_db = 20.0f * log10f(linear_gain);
  if (gain_db < kMinimumAudibleVolumeDb) {
    gain_db = kMinimumAudibleVolumeDb;
  }

  // ES8311 DAC volume spans -95.5 dB (0x00) through +32 dB (0xFF) at
  // 0.5 dB per step. Keep the slider's top at unity gain (0 dB) to avoid
  // digitally boosting and clipping the test tone.
  const int register_value = static_cast<int>(lroundf((gain_db + 95.5f) * 2.0f));
  return static_cast<uint8_t>(constrain(register_value, 0, 255));
}

bool initializeEs8311ForPlayback() {
  // Clock, I2S format and analog setup follow Espressif's ES8311 driver and
  // the FNK0104B Freenove Echo example. The latter configures this codec as an
  // I2S slave with 6.144 MHz MCLK at 16 kHz / 16-bit.
  if (!writeCodecRegister(0x00, 0x1F)) return false;
  delay(20);
  if (!writeCodecRegister(0x00, 0x00) ||
      !writeCodecRegister(0x00, 0x80) ||
      !writeCodecRegister(0x01, 0x3F)) {
    return false;
  }

  uint8_t reg02 = 0;
  uint8_t reg06 = 0;
  uint8_t reg07 = 0;
  uint8_t reg31 = 0;
  if (!readCodecRegister(0x02, reg02) || !readCodecRegister(0x06, reg06) ||
      !readCodecRegister(0x07, reg07) || !readCodecRegister(0x31, reg31)) {
    return false;
  }

  // Espressif's ES8311 coefficient table for 6.144 MHz MCLK / 16 kHz uses
  // pre-divider 3, pre-multiplier 2x, 1x ADC/DAC dividers, and BCLK divider 4.
  // Keep the reserved bits as the driver does and explicitly select normal
  // clock polarity and I2S slave format.
  const uint8_t clock_setup[][2] = {
      {0x02, static_cast<uint8_t>((reg02 & 0x07) | 0x48)},
      {0x03, 0x10},
      {0x04, 0x10},
      {0x05, 0x00},
      {0x06, static_cast<uint8_t>((reg06 & 0xC0) | 0x03)},
      {0x07, static_cast<uint8_t>(reg07 & 0xC0)},
      {0x08, 0xFF},
      {0x00, 0x80},
      {0x09, 0x0C},
      {0x0A, 0x0C},
      {0x0D, 0x01},
      {0x0E, 0x02},
      {0x12, 0x00},
      {0x13, 0x10},
      {0x1C, 0x6A},
      {0x37, 0x08},
      // Initial slider level is 85 percent, converted from perceived linear
      // amplitude to the codec's decibel-based volume register.
      {0x32, codecVolumeRegister(85)},
      // Clear the DAC mute bits while preserving the other register fields.
      {0x31, static_cast<uint8_t>(reg31 & ~0x60)},
  };
  for (const auto& entry : clock_setup) {
    if (!writeCodecRegister(entry[0], entry[1])) return false;
  }
  return true;
}

}  // namespace

SpeakerSupport speaker;

bool SpeakerSupport::begin() {
  end();
  // A previous output task must be gone before this port can be installed
  // again. Keep the amplifier disabled if teardown did not finish.
  if (output_task_.load() != nullptr || i2s_installed_) return false;
  stopping_ = false;

  // The verified FNK0104B amplifier enable is active low. Keep it disabled
  // until the codec and I2S output have both initialized successfully.
  digitalWrite(pins::audio::amplifier_enable, HIGH);
  pinMode(pins::audio::amplifier_enable, OUTPUT);

  if (!Wire.begin(pins::audio::i2c_sda, pins::audio::i2c_scl,
                  kI2cClockHz)) {
    return false;
  }
  Wire.setClock(kI2cClockHz);
  Wire.setTimeOut(50);

  i2s_config_t i2s_config = {};
  i2s_config.mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_TX);
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
  i2s_installed_ = true;

  const i2s_pin_config_t i2s_pins = {
      pins::audio::i2s_master_clock,
      pins::audio::i2s_bit_clock,
      pins::audio::i2s_word_select,
      pins::audio::i2s_data_out,
      -1,
  };
  if (i2s_set_pin(kI2sPort, &i2s_pins) != ESP_OK ||
      !initializeEs8311ForPlayback()) {
    end();
    return false;
  }

  i2s_zero_dma_buffer(kI2sPort);
  ready_ = true;
  TaskHandle_t created_task = nullptr;
  if (xTaskCreatePinnedToCore(outputTaskEntry, "speaker-output", 3072, this,
                              2, &created_task, 0) != pdPASS) {
    output_task_ = nullptr;
    end();
    return false;
  }
  output_task_ = created_task;
  digitalWrite(pins::audio::amplifier_enable, LOW);
  return true;
}

bool SpeakerSupport::setTone(uint16_t frequency_hz) {
  if (!ready_ || frequency_hz >= kSampleRate / 2) return false;
  tone_frequency_hz_ = frequency_hz;
  return true;
}

void SpeakerSupport::end() {
  pinMode(pins::audio::amplifier_enable, OUTPUT);
  digitalWrite(pins::audio::amplifier_enable, HIGH);
  ready_ = false;
  tone_frequency_hz_ = 0;
  stopping_ = true;

  // Let the output task finish its bounded I2S write before uninstalling the
  // driver. The task clears its handle only after its last I2S access.
  for (uint32_t waited_ms = 0;
       output_task_.load() != nullptr && waited_ms < 1000; ++waited_ms) {
    delay(1);
  }
  if (output_task_.load() == nullptr && i2s_installed_) {
    i2s_stop(kI2sPort);
    i2s_driver_uninstall(kI2sPort);
    i2s_installed_ = false;
  }
}

bool SpeakerSupport::setVolume(uint8_t percent) {
  if (!ready_ || percent > 100) return false;
  uint8_t reg31 = 0;
  if (!readCodecRegister(0x31, reg31)) return false;

  if (percent == 0) {
    return writeCodecRegister(0x31, static_cast<uint8_t>(reg31 | 0x60));
  }

  if (!writeCodecRegister(0x32, codecVolumeRegister(percent))) return false;
  return writeCodecRegister(0x31, static_cast<uint8_t>(reg31 & ~0x60));
}

void SpeakerSupport::outputTaskEntry(void* context) {
  static_cast<SpeakerSupport*>(context)->outputTask();
}

void SpeakerSupport::outputTask() {
  int16_t samples[kSamplesPerChunk];
  float phase = 0.0f;
  float envelope = 0.0f;
  while (!stopping_) {
    const uint16_t frequency_hz = tone_frequency_hz_;
    const float phase_step =
        2.0f * 3.14159265358979323846f * frequency_hz / kSampleRate;
    for (size_t i = 0; i < kSamplesPerChunk; ++i) {
      const float target_envelope = frequency_hz == 0 ? 0.0f : 1.0f;
      if (envelope < target_envelope) {
        envelope = fminf(1.0f, envelope + (1.0f / 128.0f));
      } else if (envelope > target_envelope) {
        envelope = fmaxf(0.0f, envelope - (1.0f / 128.0f));
      }
      samples[i] = frequency_hz == 0
                       ? 0
                       : static_cast<int16_t>(sinf(phase) * kToneAmplitude *
                                              envelope);
      phase += phase_step;
      if (phase >= 2.0f * 3.14159265358979323846f) {
        phase -= 2.0f * 3.14159265358979323846f;
      }
    }
    size_t bytes_written = 0;
    i2s_write(kI2sPort, samples, sizeof(samples), &bytes_written,
              pdMS_TO_TICKS(100));
    if (bytes_written != sizeof(samples)) vTaskDelay(1);
  }
  output_task_ = nullptr;
  vTaskDelete(nullptr);
}

bool SpeakerSupport::ready() const { return ready_; }

}  // namespace fnk0104b
