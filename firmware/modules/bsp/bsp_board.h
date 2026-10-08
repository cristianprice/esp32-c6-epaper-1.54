// Pin map and device addresses of the Waveshare ESP32-C6-ePaper-1.54.
// Taken from the Waveshare BSP (11_FactoryProgram/components/port_bsp/epaper_config.h)
// and cross-checked against the board schematic (04_Schematic).

#ifndef BSP_BOARD_H
#define BSP_BOARD_H

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/spi_master.h"
#include "esp_adc/adc_oneshot.h"

// E-paper (SSD1681 class, 200x200, 1bpp) on SPI2.
#define BSP_EPD_SPI_HOST        SPI2_HOST
#define BSP_EPD_PIN_MOSI        GPIO_NUM_5
#define BSP_EPD_PIN_SCLK        GPIO_NUM_6
#define BSP_EPD_PIN_CS          GPIO_NUM_7
#define BSP_EPD_PIN_DC          GPIO_NUM_15
#define BSP_EPD_PIN_RST         GPIO_NUM_11
#define BSP_EPD_PIN_BUSY        GPIO_NUM_10
#define BSP_EPD_SPI_HZ          (20 * 1000 * 1000)
#define BSP_EPD_WIDTH           200
#define BSP_EPD_HEIGHT          200
#define BSP_EPD_BUF_SIZE        (BSP_EPD_WIDTH * BSP_EPD_HEIGHT / 8)

// Shared I2C bus: TCA9554, PCF85063A, SHTC3 (and ES8311, unused).
#define BSP_I2C_PORT            I2C_NUM_0
#define BSP_I2C_PIN_SDA         GPIO_NUM_18
#define BSP_I2C_PIN_SCL         GPIO_NUM_8
#define BSP_I2C_HZ              400000

#define BSP_I2C_ADDR_TCA9554    0x20
#define BSP_I2C_ADDR_PCF85063   0x51
#define BSP_I2C_ADDR_SHTC3      0x70

// TCA9554 expander outputs ("EXIO").
#define BSP_EXIO_EPD_PWR        0   // EPDPower_EN: e-paper 3V3 regulator
#define BSP_EXIO_AUDIO_PWR      1   // AudioPower_EN: codec/amp regulator
#define BSP_EXIO_RTC_INT        2   // PCF85063 INT (input)
#define BSP_EXIO_PA_CTRL        3   // speaker amplifier enable
#define BSP_EXIO_LED            4   // green LED, active low
#define BSP_EXIO_VBAT_LATCH     5   // BAT_Control: keeps the board on when on battery

// Battery voltage: VBAT / 2 on GPIO0 = ADC1 channel 0.
#define BSP_BAT_ADC_UNIT        ADC_UNIT_1
#define BSP_BAT_ADC_CHANNEL     ADC_CHANNEL_0
#define BSP_BAT_DIVIDER         2.0f

// Buttons (active low).
#define BSP_PIN_BOOT_BUTTON     GPIO_NUM_9
#define BSP_PIN_PWR_BUTTON      GPIO_NUM_2

#endif // BSP_BOARD_H
