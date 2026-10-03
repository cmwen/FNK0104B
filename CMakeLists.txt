cmake_minimum_required(VERSION 3.16)
set(SDKCONFIG_DEFAULTS "${CMAKE_CURRENT_SOURCE_DIR}/apps/19-speech-diag/sdkconfig.defaults")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(fnk0104b_speech_diagnostic)
