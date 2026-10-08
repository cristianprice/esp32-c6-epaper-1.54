# Waveshare ESP32-C6-ePaper-1.54 (out-of-tree board, built with BOARD_DIR=<this dir>).
include(boards/mpconfigboard_esp32c6_common.cmake)

list(APPEND SDKCONFIG_DEFAULTS
    ${MICROPY_BOARD_DIR}/sdkconfig.board
)

# Freeze the clock application and libraries into the firmware.
set(MICROPY_FROZEN_MANIFEST ${MICROPY_BOARD_DIR}/manifest.py)

# Board support C module (`import bsp`).
if(NOT USER_C_MODULES)
    get_filename_component(_bsp_modules ${MICROPY_BOARD_DIR}/../../modules/micropython.cmake ABSOLUTE)
    set(USER_C_MODULES ${_bsp_modules})
endif()
