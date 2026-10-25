set(whistle_root "${CMAKE_SOURCE_DIR}/lib/whistle")
idf_component_register(
    SRCS "src/main.cpp" "src/screen.cpp" "${CMAKE_SOURCE_DIR}/lib/fnk0104b/src/idf_display.cpp" "${CMAKE_SOURCE_DIR}/lib/fnk0104b/src/idf_microphone.cpp"
         "${whistle_root}/src/nd_whistle.c" "${whistle_root}/src/nd_quant.c"
         "${whistle_root}/src/nd_cact.c" "${whistle_root}/src/nd_tokenizer.c"
         "${whistle_root}/src/nd_ledcmd.c"
    INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/lib/fnk0104b/src" "${whistle_root}/include"
    REQUIRES esp_lcd espressif__esp_lcd_ili9341 esp_driver_spi esp_driver_i2s esp_driver_i2c esp_driver_gpio esp_timer esp_psram esp_partition)
