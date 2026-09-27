#pragma once

// Only board mappings verified in the cited FNK0104B resources belong here.
namespace fnk0104b::pins {

namespace display {
inline constexpr int chip_select = 10;
inline constexpr int data_command = 46;
inline constexpr int clock = 12;
inline constexpr int mosi = 11;
inline constexpr int miso = 13;
inline constexpr int backlight = 45;
// Panel reset is shared with the ESP32-S3 reset signal (CHIP_PU).
}  // namespace display

namespace touch {
inline constexpr int i2c_sda = 16;
inline constexpr int i2c_scl = 15;
inline constexpr int reset = 18;
inline constexpr int interrupt = 17;
inline constexpr int i2c_address = 0x38;
}  // namespace touch

namespace sd {
inline constexpr int clock = 38;
inline constexpr int command = 40;
inline constexpr int data0 = 39;
inline constexpr int data1 = 41;
inline constexpr int data2 = 48;
inline constexpr int data3 = 47;
}  // namespace sd

namespace rgb {
inline constexpr int data = 42;
inline constexpr int count = 1;
}  // namespace rgb

namespace audio {
inline constexpr int amplifier_enable = 1;
inline constexpr int i2s_master_clock = 4;
inline constexpr int i2s_bit_clock = 5;
inline constexpr int i2s_data_out = 6;
inline constexpr int i2s_word_select = 7;
inline constexpr int i2s_data_in = 8;
}  // namespace audio

namespace system {
inline constexpr int boot_button = 0;
inline constexpr int battery_adc = 9;
inline constexpr int usb_d_minus = 19;
inline constexpr int usb_d_plus = 20;
inline constexpr int uart0_tx = 43;
inline constexpr int uart0_rx = 44;
}  // namespace system

}  // namespace fnk0104b::pins
