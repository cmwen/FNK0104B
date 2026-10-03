# Only the isolated ESP-IDF diagnostic uses CMake. Arduino apps use SCons.
idf_component_register(
    SRCS "19-speech-diag/src/main.cpp"
         "19-speech-diag/src/screen.cpp"
         "19-speech-diag/src/audio_frontend.cpp"
         "../lib/fnk0104b/src/idf_display.cpp"
         "../lib/fnk0104b/src/idf_microphone.cpp"
    INCLUDE_DIRS "../lib/fnk0104b/src"
    REQUIRES esp_lcd espressif__esp_lcd_ili9341 esp_driver_spi esp_driver_i2s esp_driver_i2c esp_driver_gpio esp_timer esp_psram espressif__esp-sr espressif__esp-dl)

# Supplement ESP-SR's kernel list for the selected Hi ESP model.
idf_component_get_property(espdl_dir espressif__esp-dl COMPONENT_DIR)
include("${espdl_dir}/cmake/compile_register.cmake")
esp_dl_compile_add_yml("${CMAKE_CURRENT_LIST_DIR}/19-speech-diag/compile_req.yml")
