#include <string.h>
#include "esp_timer.h"
#include "bsp_board.h"
#include "bsp_epd.h"

// ---------------------------------------------------------------------------
// Waveforms from the Waveshare BSP (port_display.cpp): 153 bytes LUT followed by
// gate level (0x3F), gate voltage (0x03), source voltage (0x04, 3 bytes), VCOM (0x2C).
// ---------------------------------------------------------------------------

#define LUT_SIZE 159

static const uint8_t WF_FULL_1IN54[LUT_SIZE] = {
    0x80, 0x48, 0x40, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x40, 0x48, 0x80, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x80, 0x48, 0x40, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x40, 0x48, 0x80, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xA,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x8, 0x1, 0x0, 0x8, 0x1, 0x0, 0x2,
    0xA, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x0,
    0x0, 0x0, 0x22, 0x17, 0x41, 0x0, 0x32, 0x20,
};

static const uint8_t WF_PARTIAL_1IN54[LUT_SIZE] = {
    0x0, 0x40, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x80, 0x80, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x40, 0x40, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x80, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0xF, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x1, 0x1, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x0, 0x0, 0x0, 0x02,
    0x17, 0x41, 0xB0, 0x32, 0x28,
};

// ---------------------------------------------------------------------------
// Sequencer: a byte program of the ops below, executed by bsp_epd_poll().
// ---------------------------------------------------------------------------

enum {
    OP_END = 0,
    OP_CMD,         // OP_CMD, cmd, n, data[n]
    OP_WAIT_BUSY,   // wait until BUSY is low
    OP_DELAY,       // OP_DELAY, ms
    OP_RST,         // OP_RST, level
    OP_LUT,         // OP_LUT, 0 = full / 1 = partial: waveform RAM (0x32)
    OP_LUT_REGS,    // OP_LUT_REGS, 0 = full / 1 = partial: gate/source/VCOM registers
};

// LUT load as in the BSP's EPD_SetLut(): 0x32, wait BUSY, then the voltage registers.
#define SEQ_LUT(which) OP_LUT, which, OP_WAIT_BUSY, OP_LUT_REGS, which

#define BUSY_MIN_US     1000
#define BUSY_TIMEOUT_US (10 * 1000 * 1000)

static spi_device_handle_t s_spi;
static const uint8_t *s_seq;
static size_t s_seq_len;
static size_t s_pc;
static int64_t s_wait_until;      // OP_DELAY deadline
static int64_t s_busy_since;      // OP_WAIT_BUSY start (timeout guard)
static uint8_t s_txbuf[BSP_EPD_BUF_SIZE];

// Controller addressing used by both modes (same as the BSP's full init):
// X increments, Y decrements from 199, gate scan reversed (TB=1).
#define SEQ_ADDRESSING \
    OP_CMD, 0x01, 3, 0xC7, 0x00, 0x01,  /* driver output control */ \
    OP_CMD, 0x11, 1, 0x01,              /* data entry mode */ \
    OP_CMD, 0x44, 2, 0x00, 0x18,        /* RAM X window 0..24 */ \
    OP_CMD, 0x45, 4, 0xC7, 0x00, 0x00, 0x00  /* RAM Y window 199..0 */

#define SEQ_RESET \
    OP_RST, 1, OP_DELAY, 10, OP_RST, 0, OP_DELAY, 2, OP_RST, 1, OP_DELAY, 10, OP_WAIT_BUSY

static const uint8_t SEQ_INIT_FULL[] = {
    SEQ_RESET,
    OP_CMD, 0x12, 0, OP_WAIT_BUSY,      // SW reset
    SEQ_ADDRESSING,
    OP_CMD, 0x3C, 1, 0x01,              // border waveform
    OP_CMD, 0x18, 1, 0x80,              // internal temperature sensor
    OP_CMD, 0x22, 1, 0xB1,              // load temperature + waveform
    OP_CMD, 0x20, 0, OP_WAIT_BUSY,
    SEQ_LUT(0),
    OP_END,
};

static const uint8_t SEQ_INIT_PARTIAL[] = {
    SEQ_RESET,
    SEQ_LUT(1),
    OP_CMD, 0x37, 10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00,
    OP_CMD, 0x3C, 1, 0x80,              // border waveform
    OP_CMD, 0x22, 1, 0xC0,              // enable clock + analog
    OP_CMD, 0x20, 0, OP_WAIT_BUSY,
    SEQ_ADDRESSING,
    OP_END,
};

static const uint8_t SEQ_REFRESH_FULL[] = {
    OP_CMD, 0x22, 1, 0xC7,
    OP_CMD, 0x20, 0, OP_WAIT_BUSY,
    OP_END,
};

static const uint8_t SEQ_REFRESH_PARTIAL[] = {
    OP_CMD, 0x22, 1, 0xCF,
    OP_CMD, 0x20, 0, OP_WAIT_BUSY,
    OP_END,
};

static const uint8_t SEQ_SLEEP[] = {
    OP_CMD, 0x10, 1, 0x01,              // deep sleep mode 1 (RAM retained)
    OP_DELAY, 2,
    OP_END,
};

// ---------------------------------------------------------------------------
// Low level SPI
// ---------------------------------------------------------------------------

static esp_err_t spi_write(const uint8_t *data, size_t len, bool is_data) {
    gpio_set_level(BSP_EPD_PIN_DC, is_data);
    while (len) {
        size_t chunk = len > 4000 ? 4000 : len;
        spi_transaction_t t = {
            .length = chunk * 8,
            .tx_buffer = data,
        };
        esp_err_t err = spi_device_polling_transmit(s_spi, &t);
        if (err != ESP_OK) {
            return err;
        }
        data += chunk;
        len -= chunk;
    }
    return ESP_OK;
}

static esp_err_t send_cmd(uint8_t cmd, const uint8_t *data, size_t len) {
    esp_err_t err = spi_write(&cmd, 1, false);
    if (err == ESP_OK && len) {
        err = spi_write(data, len, true);
    }
    return err;
}

static const uint8_t *lut_table(uint8_t which) {
    return which ? WF_PARTIAL_1IN54 : WF_FULL_1IN54;
}

static esp_err_t send_lut_regs(const uint8_t *lut) {
    esp_err_t err = send_cmd(0x3F, &lut[153], 1);
    if (err == ESP_OK) err = send_cmd(0x03, &lut[154], 1);
    if (err == ESP_OK) err = send_cmd(0x04, &lut[155], 3);
    if (err == ESP_OK) err = send_cmd(0x2C, &lut[158], 1);
    return err;
}

static esp_err_t write_ram(uint8_t ram_cmd, const uint8_t *img) {
    static const uint8_t x0[] = {0x00};
    static const uint8_t y0[] = {0xC7, 0x00};
    esp_err_t err = send_cmd(0x4E, x0, 1);
    if (err == ESP_OK) err = send_cmd(0x4F, y0, 2);
    if (err != ESP_OK) {
        return err;
    }
    for (size_t i = 0; i < BSP_EPD_BUF_SIZE; i++) {
        s_txbuf[i] = ~img[i];
    }
    return send_cmd(ram_cmd, s_txbuf, BSP_EPD_BUF_SIZE);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

esp_err_t bsp_epd_init_io(void) {
    if (s_spi) {
        return ESP_OK;
    }
    spi_bus_config_t bus = {
        .mosi_io_num = BSP_EPD_PIN_MOSI,
        .miso_io_num = -1,
        .sclk_io_num = BSP_EPD_PIN_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4096,
    };
    esp_err_t err = spi_bus_initialize(BSP_EPD_SPI_HOST, &bus, SPI_DMA_CH_AUTO);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }
    spi_device_interface_config_t dev = {
        .clock_speed_hz = BSP_EPD_SPI_HZ,
        .mode = 0,
        .spics_io_num = BSP_EPD_PIN_CS,
        .queue_size = 1,
    };
    err = spi_bus_add_device(BSP_EPD_SPI_HOST, &dev, &s_spi);
    if (err != ESP_OK) {
        return err;
    }
    gpio_config_t out = {
        .pin_bit_mask = (1ULL << BSP_EPD_PIN_DC) | (1ULL << BSP_EPD_PIN_RST),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    err = gpio_config(&out);
    if (err != ESP_OK) {
        return err;
    }
    gpio_set_level(BSP_EPD_PIN_RST, 1);
    gpio_config_t in = {
        .pin_bit_mask = 1ULL << BSP_EPD_PIN_BUSY,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    return gpio_config(&in);
}

static esp_err_t start(const uint8_t *seq, size_t len) {
    if (!bsp_epd_poll()) {
        return ESP_ERR_INVALID_STATE;
    }
    s_seq = seq;
    s_seq_len = len;
    s_pc = 0;
    s_wait_until = 0;
    s_busy_since = 0;
    return ESP_OK;
}

esp_err_t bsp_epd_begin(bsp_epd_mode_t mode) {
    if (mode == BSP_EPD_MODE_FULL) {
        return start(SEQ_INIT_FULL, sizeof(SEQ_INIT_FULL));
    }
    return start(SEQ_INIT_PARTIAL, sizeof(SEQ_INIT_PARTIAL));
}

esp_err_t bsp_epd_show(const uint8_t *new_img, const uint8_t *old_img, bool full) {
    if (!bsp_epd_poll()) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t err = write_ram(0x24, new_img);
    if (err == ESP_OK) {
        err = write_ram(0x26, old_img ? old_img : new_img);
    }
    if (err != ESP_OK) {
        return err;
    }
    if (full) {
        return start(SEQ_REFRESH_FULL, sizeof(SEQ_REFRESH_FULL));
    }
    return start(SEQ_REFRESH_PARTIAL, sizeof(SEQ_REFRESH_PARTIAL));
}

esp_err_t bsp_epd_sleep(void) {
    return start(SEQ_SLEEP, sizeof(SEQ_SLEEP));
}

bool bsp_epd_busy(void) {
    return gpio_get_level(BSP_EPD_PIN_BUSY) == 1;
}

bool bsp_epd_poll(void) {
    while (s_pc < s_seq_len) {
        uint8_t op = s_seq[s_pc];
        switch (op) {
            case OP_END:
                s_pc = s_seq_len;
                break;
            case OP_CMD: {
                uint8_t n = s_seq[s_pc + 2];
                send_cmd(s_seq[s_pc + 1], &s_seq[s_pc + 3], n);
                s_pc += 3 + n;
                break;
            }
            case OP_WAIT_BUSY: {
                int64_t now = esp_timer_get_time();
                if (!s_busy_since) {
                    s_busy_since = now;
                }
                if (now - s_busy_since < BUSY_MIN_US) {
                    return false;  // give BUSY time to assert after the command
                }
                if (bsp_epd_busy()) {
                    if (now - s_busy_since < BUSY_TIMEOUT_US) {
                        return false;
                    }
                    // Stuck BUSY (panel unpowered?): give up on this wait.
                }
                s_busy_since = 0;
                s_pc += 1;
                break;
            }
            case OP_DELAY: {
                int64_t now = esp_timer_get_time();
                if (!s_wait_until) {
                    s_wait_until = now + (int64_t)s_seq[s_pc + 1] * 1000;
                }
                if (now < s_wait_until) {
                    return false;
                }
                s_wait_until = 0;
                s_pc += 2;
                break;
            }
            case OP_RST:
                gpio_set_level(BSP_EPD_PIN_RST, s_seq[s_pc + 1]);
                s_pc += 2;
                break;
            case OP_LUT:
                send_cmd(0x32, lut_table(s_seq[s_pc + 1]), 153);
                s_pc += 2;
                break;
            case OP_LUT_REGS:
                send_lut_regs(lut_table(s_seq[s_pc + 1]));
                s_pc += 2;
                break;
            default:
                s_pc = s_seq_len;
                break;
        }
    }
    return true;
}
