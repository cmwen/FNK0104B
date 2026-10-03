# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0
# Based on ESP-IDF v6.0.1 tools/cmake/linker_script_preprocessor.cmake.
# PlatformIO 7.0.1 invokes this interface even when using IDF 5.5.5.
separate_arguments(CFLAGS_LIST UNIX_COMMAND "${CFLAGS}")
execute_process(COMMAND "${CC}" "-C" "-P" "-x" "c" "-E" ${CFLAGS_LIST} "${SOURCE}"
                RESULT_VARIABLE RET_CODE
                OUTPUT_VARIABLE PREPROCESSED_LINKER_SCRIPT
                ERROR_VARIABLE ERROR_VAR)
if(RET_CODE AND NOT RET_CODE EQUAL 0)
    message(FATAL_ERROR "Can't generate ${TARGET}\nRET_CODE: ${RET_CODE}\nERROR_MESSAGE: ${ERROR_VAR}")
endif()
string(REPLACE "\\n" "\n" TEXT "${PREPROCESSED_LINKER_SCRIPT}")
file(WRITE "${TARGET}" "${TEXT}")
