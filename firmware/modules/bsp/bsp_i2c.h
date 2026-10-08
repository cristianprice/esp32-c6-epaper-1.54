// Shared I2C master bus of the board (replaces the BSP's I2cMasterBus singleton).

#ifndef BSP_I2C_H
#define BSP_I2C_H

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#include "driver/i2c_master.h"

esp_err_t bsp_i2c_init(void);
i2c_master_bus_handle_t bsp_i2c_bus(void);

// Adds a 7-bit device on the bus at the board's bus speed.
esp_err_t bsp_i2c_add_device(uint8_t addr, i2c_master_dev_handle_t *out);

// Register helpers for 8-bit register devices.
esp_err_t bsp_i2c_read_reg(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t *buf, size_t len);
esp_err_t bsp_i2c_write_reg(i2c_master_dev_handle_t dev, uint8_t reg, const uint8_t *buf, size_t len);

#endif // BSP_I2C_H
