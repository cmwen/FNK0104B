#ifndef ARDUINO
#include "fnk0104b/idf_storage.hpp"
#include "fnk0104b/pins.hpp"
#include "driver/sdmmc_host.h"
#include "esp_vfs_fat.h"
namespace fnk0104b {
esp_err_t mountIdfSdCard() {
  sdmmc_host_t host = SDMMC_HOST_DEFAULT();
  host.max_freq_khz = SDMMC_FREQ_DEFAULT;
  sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
  slot.width = 4;
  slot.clk = static_cast<gpio_num_t>(pins::sd::clock);
  slot.cmd = static_cast<gpio_num_t>(pins::sd::command);
  slot.d0 = static_cast<gpio_num_t>(pins::sd::data0);
  slot.d1 = static_cast<gpio_num_t>(pins::sd::data1);
  slot.d2 = static_cast<gpio_num_t>(pins::sd::data2);
  slot.d3 = static_cast<gpio_num_t>(pins::sd::data3);
  slot.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;
  esp_vfs_fat_sdmmc_mount_config_t mount = {};
  mount.format_if_mount_failed = false;
  mount.max_files = 4;
  mount.allocation_unit_size = 16384;
  sdmmc_card_t* card = nullptr;
  return esp_vfs_fat_sdmmc_mount(kIdfSdRoot, &host, &slot, &mount, &card);
}
}
#endif
