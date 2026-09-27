#pragma once

#include <fnk0104b/pins.hpp>

#if __has_include("locallink_secrets.h")
#include "locallink_secrets.h"
#endif

#ifndef LOCALLINK_WIFI_SSID
#define LOCALLINK_WIFI_SSID ""
#endif
#ifndef LOCALLINK_WIFI_PASSWORD
#define LOCALLINK_WIFI_PASSWORD ""
#endif

// DNS-SD instance name advertised by the speech backend. This service name is
// independent of LocalLink, the app used to manage local services.
#ifndef LOCALLINK_SERVICE_INSTANCE
#define LOCALLINK_SERVICE_INSTANCE "Speech Recognition"
#endif

// The fallback is disabled until all three values are configured.
#ifndef LOCALLINK_FALLBACK_HOST
#define LOCALLINK_FALLBACK_HOST ""
#endif
#ifndef LOCALLINK_FALLBACK_PORT
#define LOCALLINK_FALLBACK_PORT 0
#endif
#ifndef LOCALLINK_FALLBACK_PATH
#define LOCALLINK_FALLBACK_PATH ""
#endif

#ifndef LOCALLINK_RECORD_SECONDS
#define LOCALLINK_RECORD_SECONDS 20
#endif

// Override any pin in this file for a custom board. FNK0104B defaults come
// from the cited board resources and are centralized in fnk0104b/pins.hpp.
#ifndef LOCALLINK_PIN_I2C_SDA
#define LOCALLINK_PIN_I2C_SDA fnk0104b::pins::audio::i2c_sda
#endif
#ifndef LOCALLINK_PIN_I2C_SCL
#define LOCALLINK_PIN_I2C_SCL fnk0104b::pins::audio::i2c_scl
#endif
#ifndef LOCALLINK_CODEC_I2C_ADDRESS
#define LOCALLINK_CODEC_I2C_ADDRESS fnk0104b::pins::audio::codec_i2c_address
#endif
#ifndef LOCALLINK_PIN_I2S_MCLK
#define LOCALLINK_PIN_I2S_MCLK fnk0104b::pins::audio::i2s_master_clock
#endif
#ifndef LOCALLINK_PIN_I2S_BCLK
#define LOCALLINK_PIN_I2S_BCLK fnk0104b::pins::audio::i2s_bit_clock
#endif
#ifndef LOCALLINK_PIN_I2S_DATA_OUT
#define LOCALLINK_PIN_I2S_DATA_OUT fnk0104b::pins::audio::i2s_data_out
#endif
#ifndef LOCALLINK_PIN_I2S_WS
#define LOCALLINK_PIN_I2S_WS fnk0104b::pins::audio::i2s_word_select
#endif
#ifndef LOCALLINK_PIN_I2S_DATA_IN
#define LOCALLINK_PIN_I2S_DATA_IN fnk0104b::pins::audio::i2s_data_in
#endif
