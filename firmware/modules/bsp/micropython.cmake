# `bsp` - MicroPython bindings for the Waveshare ESP32-C6-ePaper-1.54 board support package.
add_library(usermod_bsp INTERFACE)

target_sources(usermod_bsp INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/bsp_i2c.c
    ${CMAKE_CURRENT_LIST_DIR}/bsp_expander.c
    ${CMAKE_CURRENT_LIST_DIR}/bsp_epd.c
    ${CMAKE_CURRENT_LIST_DIR}/bsp_shtc3.c
    ${CMAKE_CURRENT_LIST_DIR}/bsp_rtc.c
    ${CMAKE_CURRENT_LIST_DIR}/bsp_battery.c
    ${CMAKE_CURRENT_LIST_DIR}/modbsp.c
)

target_include_directories(usermod_bsp INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(usermod INTERFACE usermod_bsp)
