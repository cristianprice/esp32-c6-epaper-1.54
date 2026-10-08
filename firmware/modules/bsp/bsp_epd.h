// 1.54" 200x200 e-paper (SSD1681 class), ported from the Waveshare BSP port_display.cpp.
//
// Differences from the BSP:
// - Non-blocking: every operation that waits for the BUSY pin or a delay is queued as a
//   sequence and advanced by bsp_epd_poll(), so MicroPython can `await` it.
// - No internal frame buffer: callers pass the image. Images use 1 = black (framebuf
//   convention); they are inverted on the way to the panel RAM (where 1 = white).
// - Both RAMs can be written explicitly (0x26 "old", 0x24 "new"), so a partial refresh
//   works after a deep sleep or power cycle of the panel.

#ifndef BSP_EPD_H
#define BSP_EPD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

typedef enum {
    BSP_EPD_MODE_FULL = 0,
    BSP_EPD_MODE_PARTIAL = 1,
} bsp_epd_mode_t;

// One-time SPI bus/device and GPIO setup.
esp_err_t bsp_epd_init_io(void);

// Starts the panel init sequence (HW reset, controller setup, LUT) for `mode`.
esp_err_t bsp_epd_begin(bsp_epd_mode_t mode);

// Writes the image(s) to panel RAM and starts the refresh. `old_img` may be NULL:
// then `new_img` is written to both RAMs (base image). Must be called when idle.
esp_err_t bsp_epd_show(const uint8_t *new_img, const uint8_t *old_img, bool full);

// Starts the panel deep-sleep sequence (RAM retained while powered).
esp_err_t bsp_epd_sleep(void);

// Advances the queued sequence. Returns true when idle (sequence finished).
bool bsp_epd_poll(void);

// Raw BUSY pin level (true = controller busy).
bool bsp_epd_busy(void);

#endif // BSP_EPD_H
