#pragma once

// Shared FNK0104B setup for its verified 2.8-inch ILI9341 display.
#define ILI9341_DRIVER
#define USE_FSPI_PORT
#define TFT_MISO 13
#define TFT_MOSI 11
#define TFT_SCLK 12
#define TFT_CS 10
#define TFT_DC 46
#define TFT_RST -1

#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
