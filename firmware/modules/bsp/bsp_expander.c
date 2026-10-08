#include "bsp_board.h"
#include "bsp_expander.h"
#include "bsp_i2c.h"

#define TCA9554_REG_INPUT    0x00
#define TCA9554_REG_OUTPUT   0x01
#define TCA9554_REG_POLARITY 0x02
#define TCA9554_REG_CONFIG   0x03  // 1 = input, 0 = output

static i2c_master_dev_handle_t s_dev;
static uint8_t s_output;
static uint8_t s_config;

esp_err_t bsp_expander_init(void) {
    esp_err_t err;
    if (!s_dev) {
        err = bsp_i2c_add_device(BSP_I2C_ADDR_TCA9554, &s_dev);
        if (err != ESP_OK) {
            return err;
        }
    }
    err = bsp_i2c_read_reg(s_dev, TCA9554_REG_OUTPUT, &s_output, 1);
    if (err != ESP_OK) {
        return err;
    }
    return bsp_i2c_read_reg(s_dev, TCA9554_REG_CONFIG, &s_config, 1);
}

esp_err_t bsp_expander_set(uint8_t pin, bool level) {
    uint8_t mask = 1u << (pin & 7);
    uint8_t output = level ? (s_output | mask) : (s_output & ~mask);
    esp_err_t err = bsp_i2c_write_reg(s_dev, TCA9554_REG_OUTPUT, &output, 1);
    if (err != ESP_OK) {
        return err;
    }
    s_output = output;
    if (s_config & mask) {
        uint8_t config = s_config & ~mask;
        err = bsp_i2c_write_reg(s_dev, TCA9554_REG_CONFIG, &config, 1);
        if (err != ESP_OK) {
            return err;
        }
        s_config = config;
    }
    return ESP_OK;
}

bool bsp_expander_get_output(uint8_t pin) {
    return (s_output >> (pin & 7)) & 1;
}

esp_err_t bsp_expander_get_input(uint8_t pin, bool *level) {
    uint8_t input;
    esp_err_t err = bsp_i2c_read_reg(s_dev, TCA9554_REG_INPUT, &input, 1);
    if (err == ESP_OK) {
        *level = (input >> (pin & 7)) & 1;
    }
    return err;
}
