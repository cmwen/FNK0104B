#pragma once

// Only board mappings verified in the cited FNK0104B resources belong here.
namespace fnk0104b::pins {

namespace display {
constexpr int native_width = 240;
constexpr int native_height = 320;
constexpr int chip_select = 10;
constexpr int data_command = 46;
constexpr int clock = 12;
constexpr int mosi = 11;
constexpr int miso = 13;
constexpr int backlight = 45;
// Panel reset is shared with the ESP32-S3 reset signal (CHIP_PU).
}  // namespace display

namespace touch {
constexpr int i2c_sda = 16;
constexpr int i2c_scl = 15;
constexpr int reset = 18;
constexpr int interrupt = 17;
constexpr int i2c_address = 0x38;
}  // namespace touch

namespace sd {
constexpr int clock = 38;
constexpr int command = 40;
constexpr int data0 = 39;
constexpr int data1 = 41;
constexpr int data2 = 48;
constexpr int data3 = 47;
}  // namespace sd

namespace rgb {
constexpr int data = 42;
constexpr int count = 1;
}  // namespace rgb

namespace audio {
constexpr int amplifier_enable = 1;
#ifndef FNK0104B_AUDIO_I2C_SDA
#define FNK0104B_AUDIO_I2C_SDA 16
#endif
#ifndef FNK0104B_AUDIO_I2C_SCL
#define FNK0104B_AUDIO_I2C_SCL 15
#endif
#ifndef FNK0104B_AUDIO_I2C_ADDRESS
#define FNK0104B_AUDIO_I2C_ADDRESS 0x18
#endif
#ifndef FNK0104B_AUDIO_I2S_MCLK
#define FNK0104B_AUDIO_I2S_MCLK 4
#endif
#ifndef FNK0104B_AUDIO_I2S_BCLK
#define FNK0104B_AUDIO_I2S_BCLK 5
#endif
#ifndef FNK0104B_AUDIO_I2S_DATA_OUT
#define FNK0104B_AUDIO_I2S_DATA_OUT 8
#endif
#ifndef FNK0104B_AUDIO_I2S_WS
#define FNK0104B_AUDIO_I2S_WS 7
#endif
#ifndef FNK0104B_AUDIO_I2S_DATA_IN
#define FNK0104B_AUDIO_I2S_DATA_IN 6
#endif
constexpr int i2c_sda = FNK0104B_AUDIO_I2C_SDA;
constexpr int i2c_scl = FNK0104B_AUDIO_I2C_SCL;
constexpr int codec_i2c_address = FNK0104B_AUDIO_I2C_ADDRESS;
constexpr int i2s_master_clock = FNK0104B_AUDIO_I2S_MCLK;
constexpr int i2s_bit_clock = FNK0104B_AUDIO_I2S_BCLK;
constexpr int i2s_data_out = FNK0104B_AUDIO_I2S_DATA_OUT;
constexpr int i2s_word_select = FNK0104B_AUDIO_I2S_WS;
constexpr int i2s_data_in = FNK0104B_AUDIO_I2S_DATA_IN;
}  // namespace audio

namespace system {
constexpr int boot_button = 0;
constexpr int battery_adc = 9;
constexpr int usb_d_minus = 19;
constexpr int usb_d_plus = 20;
constexpr int uart0_tx = 43;
constexpr int uart0_rx = 44;
}  // namespace system

}  // namespace fnk0104b::pins
