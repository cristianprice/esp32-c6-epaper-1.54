#include "bsp_board.h"
#include "bsp_i2c.h"
#include "bsp_rtc.h"

#define PCF85063_REG_CTRL1   0x00
#define PCF85063_REG_RAM     0x03
#define PCF85063_REG_SECONDS 0x04  // seconds .. years: 7 registers, BCD

#define CTRL1_STOP   (1 << 5)
#define CTRL1_12_24  (1 << 1)
#define SECONDS_OS   (1 << 7)      // oscillator stopped

static i2c_master_dev_handle_t s_dev;

static uint8_t bcd2bin(uint8_t v) {
    return (v >> 4) * 10 + (v & 0x0f);
}

static uint8_t bin2bcd(uint8_t v) {
    return ((v / 10) << 4) | (v % 10);
}

esp_err_t bsp_rtc_init(void) {
    esp_err_t err;
    if (!s_dev) {
        err = bsp_i2c_add_device(BSP_I2C_ADDR_PCF85063, &s_dev);
        if (err != ESP_OK) {
            return err;
        }
    }
    uint8_t ctrl1;
    err = bsp_i2c_read_reg(s_dev, PCF85063_REG_CTRL1, &ctrl1, 1);
    if (err != ESP_OK) {
        return err;
    }
    if (ctrl1 & (CTRL1_STOP | CTRL1_12_24)) {
        ctrl1 &= ~(CTRL1_STOP | CTRL1_12_24);  // running, 24h mode
        err = bsp_i2c_write_reg(s_dev, PCF85063_REG_CTRL1, &ctrl1, 1);
    }
    return err;
}

esp_err_t bsp_rtc_get(bsp_rtc_time_t *t, bool *valid) {
    uint8_t r[7];
    esp_err_t err = bsp_i2c_read_reg(s_dev, PCF85063_REG_SECONDS, r, 7);
    if (err != ESP_OK) {
        return err;
    }
    *valid = !(r[0] & SECONDS_OS);
    t->second = bcd2bin(r[0] & 0x7f);
    t->minute = bcd2bin(r[1] & 0x7f);
    t->hour = bcd2bin(r[2] & 0x3f);
    t->day = bcd2bin(r[3] & 0x3f);
    t->weekday = (uint8_t)(((r[4] & 0x07) + 6) % 7);  // chip: 0 = Sunday
    t->month = bcd2bin(r[5] & 0x1f);
    t->year = 2000 + bcd2bin(r[6]);
    return ESP_OK;
}

esp_err_t bsp_rtc_set(const bsp_rtc_time_t *t) {
    uint8_t r[7] = {
        bin2bcd(t->second),  // also clears the OS flag
        bin2bcd(t->minute),
        bin2bcd(t->hour),
        bin2bcd(t->day),
        (uint8_t)((t->weekday + 1) % 7),
        bin2bcd(t->month),
        bin2bcd((uint8_t)(t->year % 100)),
    };
    return bsp_i2c_write_reg(s_dev, PCF85063_REG_SECONDS, r, 7);
}

esp_err_t bsp_rtc_ram_get(uint8_t *value) {
    return bsp_i2c_read_reg(s_dev, PCF85063_REG_RAM, value, 1);
}

esp_err_t bsp_rtc_ram_set(uint8_t value) {
    return bsp_i2c_write_reg(s_dev, PCF85063_REG_RAM, &value, 1);
}
