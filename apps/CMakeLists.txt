idf_build_get_property(FNK_APP FNK_APP)
# PlatformIO selects the isolated IDF app; Arduino apps continue using SCons.
set(board_sources "../lib/fnk0104b/src/idf_display.cpp" "../lib/fnk0104b/src/idf_microphone.cpp")
set(board_requires esp_lcd espressif__esp_lcd_ili9341 esp_driver_spi esp_driver_i2s esp_driver_i2c esp_driver_gpio esp_timer esp_psram)
if(FNK_APP STREQUAL "codex-monitor" OR FNK_APP STREQUAL "codex-audio-diag")
    set(monitor_libdeps "${CMAKE_SOURCE_DIR}/.pio/libdeps/${FNK_APP}")
    if(FNK_APP STREQUAL "codex-audio-diag")
        set(monitor_app_sources "${CMAKE_SOURCE_DIR}/apps/25-codex-audio-diag/src/main.cpp")
    else()
        set(monitor_app_sources "${CMAKE_SOURCE_DIR}/apps/codex-monitor/src/main.cpp" "${CMAKE_SOURCE_DIR}/apps/codex-monitor/src/speech.cpp" "${CMAKE_SOURCE_DIR}/apps/codex-monitor/src/wifi_setup.cpp")
    endif()
    idf_component_register(
        SRCS ${monitor_app_sources}
             "${CMAKE_SOURCE_DIR}/lib/fnk0104b/src/usb_microphone.cpp" "${CMAKE_SOURCE_DIR}/lib/fnk0104b/src/audio_input.cpp"
             "${CMAKE_SOURCE_DIR}/lib/fnk0104b/src/codex_usb.cpp" "${CMAKE_SOURCE_DIR}/lib/codex_hid/src/backend.cpp"
             "${CMAKE_SOURCE_DIR}/lib/fnk0104b/src/board.cpp" "${CMAKE_SOURCE_DIR}/lib/fnk0104b/src/microphone.cpp"
             "${CMAKE_SOURCE_DIR}/lib/speech/src/audio_frontend.cpp" "${CMAKE_SOURCE_DIR}/lib/fnk0104b/src/speaker.cpp" "${CMAKE_SOURCE_DIR}/lib/locallink/src/protocol.cpp"
             "${monitor_libdeps}/TFT_eSPI/TFT_eSPI.cpp"
        INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/apps/codex-monitor/include" "${CMAKE_SOURCE_DIR}/lib/fnk0104b/src" "${CMAKE_SOURCE_DIR}/lib/locallink/src"
                     "${CMAKE_SOURCE_DIR}/lib/codex_hid/src" "${CMAKE_SOURCE_DIR}/lib/ui/src" "${CMAKE_SOURCE_DIR}/lib/speech/src" "${monitor_libdeps}/TFT_eSPI" "${monitor_libdeps}/ArduinoJson/src"
        REQUIRES espressif__arduino-esp32 espressif__esp-sr espressif__esp-dl espressif__network_provisioning bootloader_support arduino_tinyusb)
    target_compile_definitions(${COMPONENT_LIB} PUBLIC
        ARDUINO_USB_CDC_ON_BOOT=1 ARDUINO_USB_MODE=0 FNK0104B_ENABLE_CODEX_HID=1 FNK0104B_ENABLE_USB_AUDIO=1 FNK0104B_ENABLE_DISPLAY=1 USER_SETUP_LOADED FNK_SPEECH_EXTERNAL_MICROPHONE=1)
    # TFT_eSPI macros trigger GCC 14 indentation warnings; keep them visible.
    target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-error=misleading-indentation -include "${CMAKE_SOURCE_DIR}/lib/fnk0104b/src/tft_setup.h")
    idf_component_get_property(arduino_lib espressif__arduino-esp32 COMPONENT_LIB)
    target_compile_definitions(${arduino_lib} PUBLIC ARDUINO_USB_CDC_ON_BOOT=1
        ARDUINO_USB_MODE=0 BOARD_HAS_PSRAM)
    target_compile_options(${arduino_lib} PRIVATE -include
        "${CMAKE_SOURCE_DIR}/lib/fnk0104b/src/fnk0104b/codex_usb_identity.hpp")
    idf_component_get_property(espdl_dir espressif__esp-dl COMPONENT_DIR)
    include("${espdl_dir}/cmake/compile_register.cmake")
    esp_dl_compile_add_yml("${CMAKE_CURRENT_LIST_DIR}/19-speech-diag/compile_req.yml")
    return()
endif()
if(FNK_APP STREQUAL "speech-diag")
    set(app_sources "19-speech-diag/src/main.cpp" "19-speech-diag/src/screen.cpp" "../lib/speech/src/audio_frontend.cpp")
    set(app_requires espressif__esp-sr espressif__esp-dl)
elseif(FNK_APP STREQUAL "recorder")
    set(app_sources "20-recorder/src/main.cpp" "20-recorder/src/screen.cpp" "20-recorder/src/storage.cpp" "../lib/speech/src/audio_frontend.cpp" "../lib/fnk0104b/src/idf_storage.cpp")
    set(app_requires espressif__esp-sr espressif__esp-dl espressif__esp_audio_codec fatfs sdmmc)
elseif(FNK_APP STREQUAL "recorder-io-diag")
    set(app_sources "21-recorder-io-diag/src/main.cpp" "../lib/fnk0104b/src/idf_storage.cpp")
    set(app_requires fatfs sdmmc)
else()
    message(FATAL_ERROR "Unknown FNK_APP: ${FNK_APP}")
endif()
idf_component_register(SRCS ${app_sources} ${board_sources}
    INCLUDE_DIRS "../lib/fnk0104b/src" "../lib/speech/src" "../lib/recorder/src"
    REQUIRES ${board_requires} ${app_requires})
if(NOT FNK_APP STREQUAL "recorder-io-diag")
    idf_component_get_property(espdl_dir espressif__esp-dl COMPONENT_DIR)
    include("${espdl_dir}/cmake/compile_register.cmake")
    esp_dl_compile_add_yml("${CMAKE_CURRENT_LIST_DIR}/19-speech-diag/compile_req.yml")
endif()
