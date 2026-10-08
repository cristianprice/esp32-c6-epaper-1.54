#include "esp_rom_sys.h"
#include "bsp_board.h"
#include "bsp_i2c.h"
#include "bsp_shtc3.h"

#define SHTC3_CMD_READ_ID       0xEFC8
#define SHTC3_CMD_SOFT_RESET    0x805D
#define SHTC3_CMD_SLEEP         0xB098
#define SHTC3_CMD_WAKEUP        0x3517
#define SHTC3_CMD_MEAS_T_RH     0x7866  // T first, no clock stretching (polling)

#define SHTC3_TIMEOUT_MS 50

static i2c_master_dev_handle_t s_dev;
static uint16_t s_id;

static esp_err_t write_cmd(uint16_t cmd) {
    uint8_t buf[2] = {cmd >> 8, cmd & 0xff};
    return i2c_master_transmit(s_dev, buf, 2, SHTC3_TIMEOUT_MS);
}

static uint8_t crc8(const uint8_t *data, int len) {
    uint8_t crc = 0xFF;
    for (int i = 0; i < len; i++) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; bit++) {
            crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x31) : (uint8_t)(crc << 1);
        }
    }
    return crc;
}

esp_err_t bsp_shtc3_init(void) {
    esp_err_t err;
    if (!s_dev) {
        err = bsp_i2c_add_device(BSP_I2C_ADDR_SHTC3, &s_dev);
        if (err != ESP_OK) {
            return err;
        }
    }
    err = bsp_shtc3_wakeup();
    if (err != ESP_OK) {
        return err;
    }
    uint8_t cmd[2] = {SHTC3_CMD_READ_ID >> 8, SHTC3_CMD_READ_ID & 0xff};
    uint8_t rx[3];
    err = i2c_master_transmit_receive(s_dev, cmd, 2, rx, 3, SHTC3_TIMEOUT_MS);
    if (err != ESP_OK) {
        return err;
    }
    if (crc8(rx, 2) != rx[2]) {
        return ESP_ERR_INVALID_CRC;
    }
    s_id = (rx[0] << 8) | rx[1];
    return ESP_OK;
}

uint16_t bsp_shtc3_id(void) {
    return s_id;
}

esp_err_t bsp_shtc3_wakeup(void) {
    esp_err_t err = write_cmd(SHTC3_CMD_WAKEUP);
    esp_rom_delay_us(240);  // datasheet: max wake-up time
    return err;
}

esp_err_t bsp_shtc3_sleep(void) {
    return write_cmd(SHTC3_CMD_SLEEP);
}

esp_err_t bsp_shtc3_measure_start(void) {
    return write_cmd(SHTC3_CMD_MEAS_T_RH);
}

esp_err_t bsp_shtc3_read(float *temp_c, float *rh) {
    uint8_t rx[6];
    esp_err_t err = i2c_master_receive(s_dev, rx, 6, SHTC3_TIMEOUT_MS);
    if (err != ESP_OK) {
        return err;  // NACK while the conversion is still running
    }
    if (crc8(&rx[0], 2) != rx[2] || crc8(&rx[3], 2) != rx[5]) {
        return ESP_ERR_INVALID_CRC;
    }
    uint16_t raw_t = (rx[0] << 8) | rx[1];
    uint16_t raw_rh = (rx[3] << 8) | rx[4];
    *temp_c = 175.0f * raw_t / 65536.0f - 45.0f;
    *rh = 100.0f * raw_rh / 65536.0f;
    return ESP_OK;
}
