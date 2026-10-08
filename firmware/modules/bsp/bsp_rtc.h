// PCF85063A real-time clock (replaces the waveshare/pcf85063a managed component).
// The chip is battery backed on this board, so it keeps time across deep sleep,
// resets and USB unplugging (with a battery attached). It always runs in 24h mode.

#ifndef BSP_RTC_H
#define BSP_RTC_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

typedef struct {
    uint16_t year;      // 2000..2099
    uint8_t month;      // 1..12
    uint8_t day;        // 1..31
    uint8_t weekday;    // 0 = Monday .. 6 = Sunday (MicroPython convention)
    uint8_t hour;       // 0..23
    uint8_t minute;
    uint8_t second;
} bsp_rtc_time_t;

esp_err_t bsp_rtc_init(void);
// `valid` is false when the oscillator stopped (power loss) since the last set.
esp_err_t bsp_rtc_get(bsp_rtc_time_t *t, bool *valid);
esp_err_t bsp_rtc_set(const bsp_rtc_time_t *t);
// The single free RAM byte of the chip.
esp_err_t bsp_rtc_ram_get(uint8_t *value);
esp_err_t bsp_rtc_ram_set(uint8_t value);

#endif // BSP_RTC_H
