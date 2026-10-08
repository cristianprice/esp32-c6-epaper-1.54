#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "bsp_board.h"
#include "bsp_battery.h"

#define BAT_SAMPLES     8
#define BAT_VOLT_EMPTY  3.00f
#define BAT_VOLT_FULL   4.12f

static adc_oneshot_unit_handle_t s_adc;
static adc_cali_handle_t s_cali;

esp_err_t bsp_battery_init(void) {
    if (s_adc) {
        return ESP_OK;
    }
    adc_cali_curve_fitting_config_t cali_cfg = {
        .unit_id = BSP_BAT_ADC_UNIT,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    esp_err_t err = adc_cali_create_scheme_curve_fitting(&cali_cfg, &s_cali);
    if (err != ESP_OK) {
        return err;
    }
    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = BSP_BAT_ADC_UNIT,
    };
    err = adc_oneshot_new_unit(&unit_cfg, &s_adc);
    if (err != ESP_OK) {
        return err;
    }
    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    return adc_oneshot_config_channel(s_adc, BSP_BAT_ADC_CHANNEL, &chan_cfg);
}

esp_err_t bsp_battery_voltage(float *volts) {
    int sum_mv = 0;
    for (int i = 0; i < BAT_SAMPLES; i++) {
        int raw, mv;
        esp_err_t err = adc_oneshot_read(s_adc, BSP_BAT_ADC_CHANNEL, &raw);
        if (err == ESP_OK) {
            err = adc_cali_raw_to_voltage(s_cali, raw, &mv);
        }
        if (err != ESP_OK) {
            return err;
        }
        sum_mv += mv;
    }
    *volts = (sum_mv / (float)BAT_SAMPLES) * BSP_BAT_DIVIDER / 1000.0f;
    return ESP_OK;
}

uint8_t bsp_battery_percent(float volts) {
    if (volts <= BAT_VOLT_EMPTY) {
        return 0;
    }
    if (volts >= BAT_VOLT_FULL) {
        return 100;
    }
    return (uint8_t)((volts - BAT_VOLT_EMPTY) / (BAT_VOLT_FULL - BAT_VOLT_EMPTY) * 100.0f);
}
