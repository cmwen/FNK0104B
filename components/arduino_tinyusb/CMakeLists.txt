if(CONFIG_TINYUSB_ENABLED)

  include(FetchContent)
  # Pin the source; never follow upstream master during a firmware build.
  FetchContent_Declare(fnk_tinyusb
    URL https://codeload.github.com/hathach/tinyusb/tar.gz/c391fe9cb05abea9058ea3fc781c64e11bd057ab
    URL_HASH SHA256=f96039275a6545febe16a3ac296f545f213445f31fbd77173e5bf9e8fe43ebdf
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
  FetchContent_GetProperties(fnk_tinyusb)
  if(NOT fnk_tinyusb_POPULATED)
    FetchContent_Populate(fnk_tinyusb)
  endif()

  ### variables ###
  #################

  if(IDF_TARGET STREQUAL "esp32s2")
    set(compile_options
        "-DCFG_TUSB_MCU=OPT_MCU_ESP32S2"
        "-DCFG_TUSB_DEBUG=${CONFIG_TINYUSB_DEBUG_LEVEL}"
        "-Wno-type-limits" # needed for the vanila tinyusb with turned off classes
    )
  elseif(IDF_TARGET STREQUAL "esp32s3")
    set(compile_options
        "-DCFG_TUSB_MCU=OPT_MCU_ESP32S3"
        "-DCFG_TUSB_DEBUG=${CONFIG_TINYUSB_DEBUG_LEVEL}"
        "-Wno-type-limits" # needed for the vanila tinyusb with turned off classes
    )
  elseif(IDF_TARGET STREQUAL "esp32p4")
    set(compile_options
        "-DCFG_TUSB_MCU=OPT_MCU_ESP32P4"
        "-DCFG_TUSB_DEBUG=${CONFIG_TINYUSB_DEBUG_LEVEL}"
        "-Wno-type-limits" # needed for the vanila tinyusb with turned off classes
    )
  endif()

  set(srcs
      # tusb:
      "${fnk_tinyusb_SOURCE_DIR}/src/portable/synopsys/dwc2/dcd_dwc2.c"
      "${fnk_tinyusb_SOURCE_DIR}/src/portable/synopsys/dwc2/dwc2_common.c"
      "${fnk_tinyusb_SOURCE_DIR}/src/portable/synopsys/dwc2/hcd_dwc2.c"
      # common
      "${fnk_tinyusb_SOURCE_DIR}/src/tusb.c"
      "${fnk_tinyusb_SOURCE_DIR}/src/common/tusb_fifo.c"
      # device
      "${fnk_tinyusb_SOURCE_DIR}/src/device/usbd.c"
      "${fnk_tinyusb_SOURCE_DIR}/src/class/cdc/cdc_device.c"
      "${fnk_tinyusb_SOURCE_DIR}/src/class/hid/hid_device.c"
      "${fnk_tinyusb_SOURCE_DIR}/src/class/midi/midi_device.c"
      "${fnk_tinyusb_SOURCE_DIR}/src/class/msc/msc_device.c"
      "${fnk_tinyusb_SOURCE_DIR}/src/class/audio/audio_device.c"
      "${fnk_tinyusb_SOURCE_DIR}/src/class/video/video_device.c"
      "${fnk_tinyusb_SOURCE_DIR}/src/class/dfu/dfu_rt_device.c"
      "${fnk_tinyusb_SOURCE_DIR}/src/class/dfu/dfu_device.c"
      "${fnk_tinyusb_SOURCE_DIR}/src/class/vendor/vendor_device.c"
      "${fnk_tinyusb_SOURCE_DIR}/src/class/net/ncm_device.c"
      "${fnk_tinyusb_SOURCE_DIR}/src/class/net/ecm_rndis_device.c"
      "${fnk_tinyusb_SOURCE_DIR}/src/class/usbtmc/usbtmc_device.c"
      # host
      "${fnk_tinyusb_SOURCE_DIR}/src/host/usbh.c"
      "${fnk_tinyusb_SOURCE_DIR}/src/host/hub.c"
      "${fnk_tinyusb_SOURCE_DIR}/src/class/cdc/cdc_host.c"
      "${fnk_tinyusb_SOURCE_DIR}/src/class/hid/hid_host.c"
      "${fnk_tinyusb_SOURCE_DIR}/src/class/midi/midi_host.c"
      "${fnk_tinyusb_SOURCE_DIR}/src/class/msc/msc_host.c"
      )

  set(includes_private
      # tusb:
      #"${fnk_tinyusb_SOURCE_DIR}/lib/networking/"
      "${fnk_tinyusb_SOURCE_DIR}/hw/bsp/"
      "${fnk_tinyusb_SOURCE_DIR}/src/"
      "${fnk_tinyusb_SOURCE_DIR}/src/device"
      "${fnk_tinyusb_SOURCE_DIR}/src/host"
      "${fnk_tinyusb_SOURCE_DIR}/src/portable/synopsys/dwc2"
      )

  idf_component_get_property(FREERTOS_ORIG_INCLUDE_PATH freertos
                             ORIG_INCLUDE_PATH)
  set(includes_public
      # tusb:
      "${FREERTOS_ORIG_INCLUDE_PATH}"
      "${fnk_tinyusb_SOURCE_DIR}/src/"
      # espressif:
      "${COMPONENT_DIR}/include")

  set(requires esp_rom freertos soc)
  set(priv_requires espressif__arduino-esp32)

  idf_component_register(
    INCLUDE_DIRS ${includes_public}
    PRIV_INCLUDE_DIRS ${includes_private}
    SRCS ${srcs}
    REQUIRES ${requires}
    PRIV_REQUIRES ${priv_requires}
    )
  target_compile_options(${COMPONENT_TARGET} PRIVATE ${compile_options})

else()

  idf_component_register()

endif()
