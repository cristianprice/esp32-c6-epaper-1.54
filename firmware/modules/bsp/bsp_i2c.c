#include <string.h>
#include "bsp_board.h"
#include "bsp_i2c.h"

#define BSP_I2C_TIMEOUT_MS 50
#define BSP_I2C_MAX_WRITE  32

static i2c_master_bus_handle_t s_bus;

esp_err_t bsp_i2c_init(void) {
    if (s_bus) {
        return ESP_OK;
    }
    i2c_master_bus_config_t cfg = {
        .i2c_port = BSP_I2C_PORT,
        .sda_io_num = BSP_I2C_PIN_SDA,
        .scl_io_num = BSP_I2C_PIN_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    return i2c_new_master_bus(&cfg, &s_bus);
}

i2c_master_bus_handle_t bsp_i2c_bus(void) {
    return s_bus;
}

esp_err_t bsp_i2c_add_device(uint8_t addr, i2c_master_dev_handle_t *out) {
    i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr,
        .scl_speed_hz = BSP_I2C_HZ,
    };
    return i2c_master_bus_add_device(s_bus, &cfg, out);
}

esp_err_t bsp_i2c_read_reg(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t *buf, size_t len) {
    return i2c_master_transmit_receive(dev, &reg, 1, buf, len, BSP_I2C_TIMEOUT_MS);
}

esp_err_t bsp_i2c_write_reg(i2c_master_dev_handle_t dev, uint8_t reg, const uint8_t *buf, size_t len) {
    uint8_t tmp[BSP_I2C_MAX_WRITE + 1];
    if (len > BSP_I2C_MAX_WRITE) {
        return ESP_ERR_INVALID_SIZE;
    }
    tmp[0] = reg;
    memcpy(&tmp[1], buf, len);
    return i2c_master_transmit(dev, tmp, len + 1, BSP_I2C_TIMEOUT_MS);
}
