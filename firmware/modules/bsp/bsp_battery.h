// Battery voltage via ADC1 channel 0 (GPIO0, VBAT/2), ported from the BSP port_adc.cpp.

#ifndef BSP_BATTERY_H
#define BSP_BATTERY_H

#include <stdint.h>
#include "esp_err.h"

esp_err_t bsp_battery_init(void);
esp_err_t bsp_battery_voltage(float *volts);   // averaged over a few samples
uint8_t bsp_battery_percent(float volts);       // BSP curve: 3.00 V = 0 %, 4.12 V = 100 %

#endif // BSP_BATTERY_H
