// SHTC3 temperature/humidity sensor, ported from the Waveshare BSP port_shtc3.cpp.
// The measurement is split into start + read (the BSP blocks 20 ms in between) so
// MicroPython can await the conversion time (max 12.1 ms in normal mode).
// Unlike the BSP, no fixed -4 °C self-heating offset is applied here; the
// application applies its own configurable offset.

#ifndef BSP_SHTC3_H
#define BSP_SHTC3_H

#include <stdint.h>
#include "esp_err.h"

#define BSP_SHTC3_MEASURE_MS 13

esp_err_t bsp_shtc3_init(void);         // adds the device, reads the ID
uint16_t bsp_shtc3_id(void);
esp_err_t bsp_shtc3_wakeup(void);       // needs 240 us before the next command
esp_err_t bsp_shtc3_sleep(void);
esp_err_t bsp_shtc3_measure_start(void);
esp_err_t bsp_shtc3_read(float *temp_c, float *rh);  // ESP_ERR_INVALID_CRC on CRC error

#endif // BSP_SHTC3_H
