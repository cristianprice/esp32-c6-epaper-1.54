// TCA9554 8-bit I/O expander (power rails, LED, battery latch).
// Replaces esp_io_expander_tca9554 + its gpio_set_level() wrapper used by the BSP:
// the expander is driven directly, so machine.Pin keeps working normally.

#ifndef BSP_EXPANDER_H
#define BSP_EXPANDER_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

// Reads the current output/config registers so that re-initialising after a
// deep-sleep wake never glitches outputs (the expander keeps its state while
// the ESP32 sleeps).
esp_err_t bsp_expander_init(void);

// Drives pin `pin` (0..7) as an output at `level`. Output value is written
// before the direction, so a pin switching from input never glitches.
esp_err_t bsp_expander_set(uint8_t pin, bool level);

// Reads back the output latch value of `pin`.
bool bsp_expander_get_output(uint8_t pin);

// Reads the input level of `pin`.
esp_err_t bsp_expander_get_input(uint8_t pin, bool *level);

#endif // BSP_EXPANDER_H
