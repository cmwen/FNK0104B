#pragma once
#include "esp_err.h"
namespace fnk0104b {
// Mount existing FAT card on the verified four-bit SDIO bus. Never format.
esp_err_t mountIdfSdCard();
constexpr const char* kIdfSdRoot = "/sdcard";
}
