// `bsp` MicroPython module: the Waveshare ESP32-C6-ePaper-1.54 board support package.
//
//   import bsp
//   bsp.init()                      # I2C bus + TCA9554 expander
//   bsp.power.epd(True)             # power rails / LED / battery latch
//   bsp.epd.init(); bsp.epd.begin(full); bsp.epd.show(new, old, full); bsp.epd.poll()
//   bsp.shtc3.init(); bsp.shtc3.measure_start(); ...; bsp.shtc3.read()
//   bsp.rtc.init(); bsp.rtc.datetime(); bsp.rtc.valid()
//   bsp.battery.init(); bsp.battery.voltage(); bsp.battery.percent()
//   bsp.i2c.scan(); bsp.i2c.readfrom_mem(addr, reg, n); bsp.i2c.writeto_mem(addr, reg, buf)
//
// Note: the board I2C bus (I2C_NUM_0, SDA 18 / SCL 8) is owned by this module;
// don't create machine.I2C(0) on the same pins.

#include "py/runtime.h"
#include "py/mperrno.h"
#include "mphalport.h"

#include "bsp_board.h"
#include "bsp_i2c.h"
#include "bsp_expander.h"
#include "bsp_epd.h"
#include "bsp_shtc3.h"
#include "bsp_rtc.h"
#include "bsp_battery.h"

#define BSP_LED_ACTIVE_LOW 1

static bool s_bus_ready;

static void require_bus(void) {
    if (!s_bus_ready) {
        mp_raise_msg(&mp_type_RuntimeError, MP_ERROR_TEXT("call bsp.init() first"));
    }
}

// ---------------------------------------------------------------------------
// bsp.init()
// ---------------------------------------------------------------------------

static mp_obj_t bsp_init(void) {
    check_esp_err(bsp_i2c_init());
    check_esp_err(bsp_expander_init());
    s_bus_ready = true;
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(bsp_init_obj, bsp_init);

// ---------------------------------------------------------------------------
// bsp.power
// ---------------------------------------------------------------------------

static mp_obj_t power_rail(size_t n_args, const mp_obj_t *args, uint8_t pin, bool active_low) {
    require_bus();
    if (n_args == 0) {
        return mp_obj_new_bool(bsp_expander_get_output(pin) != active_low);
    }
    check_esp_err(bsp_expander_set(pin, mp_obj_is_true(args[0]) != active_low));
    return mp_const_none;
}

static mp_obj_t power_epd(size_t n_args, const mp_obj_t *args) {
    return power_rail(n_args, args, BSP_EXIO_EPD_PWR, false);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(power_epd_obj, 0, 1, power_epd);

static mp_obj_t power_audio(size_t n_args, const mp_obj_t *args) {
    // Codec regulator and speaker amplifier are switched together
    // (amplifier off before the rail goes down, on after it comes up).
    if (n_args == 0) {
        return power_rail(0, args, BSP_EXIO_AUDIO_PWR, false);
    }
    uint8_t first = mp_obj_is_true(args[0]) ? BSP_EXIO_AUDIO_PWR : BSP_EXIO_PA_CTRL;
    uint8_t second = mp_obj_is_true(args[0]) ? BSP_EXIO_PA_CTRL : BSP_EXIO_AUDIO_PWR;
    power_rail(1, args, first, false);
    return power_rail(1, args, second, false);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(power_audio_obj, 0, 1, power_audio);

static mp_obj_t power_vbat_latch(size_t n_args, const mp_obj_t *args) {
    return power_rail(n_args, args, BSP_EXIO_VBAT_LATCH, false);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(power_vbat_latch_obj, 0, 1, power_vbat_latch);

static mp_obj_t power_led(size_t n_args, const mp_obj_t *args) {
    return power_rail(n_args, args, BSP_EXIO_LED, BSP_LED_ACTIVE_LOW);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(power_led_obj, 0, 1, power_led);

static const mp_rom_map_elem_t power_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_power) },
    { MP_ROM_QSTR(MP_QSTR_epd), MP_ROM_PTR(&power_epd_obj) },
    { MP_ROM_QSTR(MP_QSTR_audio), MP_ROM_PTR(&power_audio_obj) },
    { MP_ROM_QSTR(MP_QSTR_vbat_latch), MP_ROM_PTR(&power_vbat_latch_obj) },
    { MP_ROM_QSTR(MP_QSTR_led), MP_ROM_PTR(&power_led_obj) },
};
static MP_DEFINE_CONST_DICT(power_globals, power_globals_table);
static const mp_obj_module_t bsp_power_module = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&power_globals,
};

// ---------------------------------------------------------------------------
// bsp.i2c - access to the shared bus for extra devices on the expansion header
// ---------------------------------------------------------------------------

static mp_obj_t i2c_scan(void) {
    require_bus();
    mp_obj_t list = mp_obj_new_list(0, NULL);
    for (int addr = 0x08; addr < 0x78; addr++) {
        if (i2c_master_probe(bsp_i2c_bus(), addr, 20) == ESP_OK) {
            mp_obj_list_append(list, MP_OBJ_NEW_SMALL_INT(addr));
        }
    }
    return list;
}
static MP_DEFINE_CONST_FUN_OBJ_0(i2c_scan_obj, i2c_scan);

static mp_obj_t i2c_readfrom_mem(mp_obj_t addr_in, mp_obj_t reg_in, mp_obj_t n_in) {
    require_bus();
    i2c_master_dev_handle_t dev;
    check_esp_err(bsp_i2c_add_device(mp_obj_get_int(addr_in), &dev));
    vstr_t vstr;
    vstr_init_len(&vstr, mp_obj_get_int(n_in));
    esp_err_t err = bsp_i2c_read_reg(dev, mp_obj_get_int(reg_in), (uint8_t *)vstr.buf, vstr.len);
    i2c_master_bus_rm_device(dev);
    if (err != ESP_OK) {
        vstr_clear(&vstr);
        check_esp_err(err);
    }
    return mp_obj_new_bytes_from_vstr(&vstr);
}
static MP_DEFINE_CONST_FUN_OBJ_3(i2c_readfrom_mem_obj, i2c_readfrom_mem);

static mp_obj_t i2c_writeto_mem(mp_obj_t addr_in, mp_obj_t reg_in, mp_obj_t buf_in) {
    require_bus();
    mp_buffer_info_t buf;
    mp_get_buffer_raise(buf_in, &buf, MP_BUFFER_READ);
    i2c_master_dev_handle_t dev;
    check_esp_err(bsp_i2c_add_device(mp_obj_get_int(addr_in), &dev));
    esp_err_t err = bsp_i2c_write_reg(dev, mp_obj_get_int(reg_in), buf.buf, buf.len);
    i2c_master_bus_rm_device(dev);
    check_esp_err(err);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_3(i2c_writeto_mem_obj, i2c_writeto_mem);

static const mp_rom_map_elem_t i2c_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_i2c) },
    { MP_ROM_QSTR(MP_QSTR_scan), MP_ROM_PTR(&i2c_scan_obj) },
    { MP_ROM_QSTR(MP_QSTR_readfrom_mem), MP_ROM_PTR(&i2c_readfrom_mem_obj) },
    { MP_ROM_QSTR(MP_QSTR_writeto_mem), MP_ROM_PTR(&i2c_writeto_mem_obj) },
};
static MP_DEFINE_CONST_DICT(i2c_globals, i2c_globals_table);
static const mp_obj_module_t bsp_i2c_module = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&i2c_globals,
};

// ---------------------------------------------------------------------------
// bsp.epd
// ---------------------------------------------------------------------------

static const uint8_t *epd_image(mp_obj_t obj) {
    mp_buffer_info_t buf;
    mp_get_buffer_raise(obj, &buf, MP_BUFFER_READ);
    if (buf.len < BSP_EPD_BUF_SIZE) {
        mp_raise_ValueError(MP_ERROR_TEXT("image must be 5000 bytes (200x200 MONO_HLSB)"));
    }
    return buf.buf;
}

static void epd_check(esp_err_t err) {
    if (err == ESP_ERR_INVALID_STATE) {
        mp_raise_OSError(MP_EBUSY);
    }
    check_esp_err(err);
}

static mp_obj_t epd_init(void) {
    check_esp_err(bsp_epd_init_io());
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(epd_init_obj, epd_init);

static mp_obj_t epd_begin(mp_obj_t full_in) {
    epd_check(bsp_epd_begin(mp_obj_is_true(full_in) ? BSP_EPD_MODE_FULL : BSP_EPD_MODE_PARTIAL));
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(epd_begin_obj, epd_begin);

static mp_obj_t epd_show(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kw_args) {
    enum { ARG_new, ARG_old, ARG_full };
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_new, MP_ARG_REQUIRED | MP_ARG_OBJ, {.u_obj = MP_OBJ_NULL} },
        { MP_QSTR_old, MP_ARG_OBJ, {.u_obj = mp_const_none} },
        { MP_QSTR_full, MP_ARG_BOOL, {.u_bool = false} },
    };
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args, pos_args, kw_args, MP_ARRAY_SIZE(allowed_args), allowed_args, args);
    const uint8_t *new_img = epd_image(args[ARG_new].u_obj);
    const uint8_t *old_img = args[ARG_old].u_obj == mp_const_none ? NULL : epd_image(args[ARG_old].u_obj);
    epd_check(bsp_epd_show(new_img, old_img, args[ARG_full].u_bool));
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_KW(epd_show_obj, 1, epd_show);

static mp_obj_t epd_sleep(void) {
    epd_check(bsp_epd_sleep());
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(epd_sleep_obj, epd_sleep);

static mp_obj_t epd_poll(void) {
    return mp_obj_new_bool(bsp_epd_poll());
}
static MP_DEFINE_CONST_FUN_OBJ_0(epd_poll_obj, epd_poll);

static mp_obj_t epd_busy(void) {
    return mp_obj_new_bool(bsp_epd_busy());
}
static MP_DEFINE_CONST_FUN_OBJ_0(epd_busy_obj, epd_busy);

static const mp_rom_map_elem_t epd_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_epd) },
    { MP_ROM_QSTR(MP_QSTR_init), MP_ROM_PTR(&epd_init_obj) },
    { MP_ROM_QSTR(MP_QSTR_begin), MP_ROM_PTR(&epd_begin_obj) },
    { MP_ROM_QSTR(MP_QSTR_show), MP_ROM_PTR(&epd_show_obj) },
    { MP_ROM_QSTR(MP_QSTR_sleep), MP_ROM_PTR(&epd_sleep_obj) },
    { MP_ROM_QSTR(MP_QSTR_poll), MP_ROM_PTR(&epd_poll_obj) },
    { MP_ROM_QSTR(MP_QSTR_busy), MP_ROM_PTR(&epd_busy_obj) },
    { MP_ROM_QSTR(MP_QSTR_WIDTH), MP_ROM_INT(BSP_EPD_WIDTH) },
    { MP_ROM_QSTR(MP_QSTR_HEIGHT), MP_ROM_INT(BSP_EPD_HEIGHT) },
};
static MP_DEFINE_CONST_DICT(epd_globals, epd_globals_table);
static const mp_obj_module_t bsp_epd_module = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&epd_globals,
};

// ---------------------------------------------------------------------------
// bsp.shtc3
// ---------------------------------------------------------------------------

static mp_obj_t shtc3_init(void) {
    require_bus();
    check_esp_err(bsp_shtc3_init());
    return MP_OBJ_NEW_SMALL_INT(bsp_shtc3_id());
}
static MP_DEFINE_CONST_FUN_OBJ_0(shtc3_init_obj, shtc3_init);

static mp_obj_t shtc3_wakeup(void) {
    check_esp_err(bsp_shtc3_wakeup());
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(shtc3_wakeup_obj, shtc3_wakeup);

static mp_obj_t shtc3_sleep(void) {
    check_esp_err(bsp_shtc3_sleep());
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(shtc3_sleep_obj, shtc3_sleep);

static mp_obj_t shtc3_measure_start(void) {
    check_esp_err(bsp_shtc3_measure_start());
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(shtc3_measure_start_obj, shtc3_measure_start);

static mp_obj_t shtc3_read(void) {
    float t, rh;
    check_esp_err(bsp_shtc3_read(&t, &rh));
    mp_obj_t tuple[2] = { mp_obj_new_float(t), mp_obj_new_float(rh) };
    return mp_obj_new_tuple(2, tuple);
}
static MP_DEFINE_CONST_FUN_OBJ_0(shtc3_read_obj, shtc3_read);

static const mp_rom_map_elem_t shtc3_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_shtc3) },
    { MP_ROM_QSTR(MP_QSTR_init), MP_ROM_PTR(&shtc3_init_obj) },
    { MP_ROM_QSTR(MP_QSTR_wakeup), MP_ROM_PTR(&shtc3_wakeup_obj) },
    { MP_ROM_QSTR(MP_QSTR_sleep), MP_ROM_PTR(&shtc3_sleep_obj) },
    { MP_ROM_QSTR(MP_QSTR_measure_start), MP_ROM_PTR(&shtc3_measure_start_obj) },
    { MP_ROM_QSTR(MP_QSTR_read), MP_ROM_PTR(&shtc3_read_obj) },
    { MP_ROM_QSTR(MP_QSTR_MEASURE_MS), MP_ROM_INT(BSP_SHTC3_MEASURE_MS) },
};
static MP_DEFINE_CONST_DICT(shtc3_globals, shtc3_globals_table);
static const mp_obj_module_t bsp_shtc3_module = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&shtc3_globals,
};

// ---------------------------------------------------------------------------
// bsp.rtc
// ---------------------------------------------------------------------------

static mp_obj_t rtc_init(void) {
    require_bus();
    check_esp_err(bsp_rtc_init());
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(rtc_init_obj, rtc_init);

// datetime() -> (year, month, day, weekday, hour, minute, second)
// datetime(tuple) sets the time (weekday 0 = Monday, as in machine.RTC).
static mp_obj_t rtc_datetime(size_t n_args, const mp_obj_t *args) {
    bsp_rtc_time_t t;
    if (n_args == 0) {
        bool valid;
        check_esp_err(bsp_rtc_get(&t, &valid));
        mp_obj_t tuple[7] = {
            MP_OBJ_NEW_SMALL_INT(t.year), MP_OBJ_NEW_SMALL_INT(t.month),
            MP_OBJ_NEW_SMALL_INT(t.day), MP_OBJ_NEW_SMALL_INT(t.weekday),
            MP_OBJ_NEW_SMALL_INT(t.hour), MP_OBJ_NEW_SMALL_INT(t.minute),
            MP_OBJ_NEW_SMALL_INT(t.second),
        };
        return mp_obj_new_tuple(7, tuple);
    }
    mp_obj_t *items;
    size_t len;
    mp_obj_get_array(args[0], &len, &items);
    if (len < 7) {
        mp_raise_ValueError(MP_ERROR_TEXT("need (year, month, day, weekday, hour, minute, second)"));
    }
    t.year = mp_obj_get_int(items[0]);
    t.month = mp_obj_get_int(items[1]);
    t.day = mp_obj_get_int(items[2]);
    t.weekday = mp_obj_get_int(items[3]);
    t.hour = mp_obj_get_int(items[4]);
    t.minute = mp_obj_get_int(items[5]);
    t.second = mp_obj_get_int(items[6]);
    if (t.year < 2000 || t.year > 2099) {
        mp_raise_ValueError(MP_ERROR_TEXT("year out of range"));
    }
    check_esp_err(bsp_rtc_set(&t));
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(rtc_datetime_obj, 0, 1, rtc_datetime);

// valid() -> False if the oscillator stopped since the time was last set.
static mp_obj_t rtc_valid(void) {
    bsp_rtc_time_t t;
    bool valid;
    check_esp_err(bsp_rtc_get(&t, &valid));
    return mp_obj_new_bool(valid);
}
static MP_DEFINE_CONST_FUN_OBJ_0(rtc_valid_obj, rtc_valid);

static mp_obj_t rtc_ram(size_t n_args, const mp_obj_t *args) {
    if (n_args == 0) {
        uint8_t v;
        check_esp_err(bsp_rtc_ram_get(&v));
        return MP_OBJ_NEW_SMALL_INT(v);
    }
    check_esp_err(bsp_rtc_ram_set(mp_obj_get_int(args[0]) & 0xff));
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(rtc_ram_obj, 0, 1, rtc_ram);

static const mp_rom_map_elem_t rtc_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_rtc) },
    { MP_ROM_QSTR(MP_QSTR_init), MP_ROM_PTR(&rtc_init_obj) },
    { MP_ROM_QSTR(MP_QSTR_datetime), MP_ROM_PTR(&rtc_datetime_obj) },
    { MP_ROM_QSTR(MP_QSTR_valid), MP_ROM_PTR(&rtc_valid_obj) },
    { MP_ROM_QSTR(MP_QSTR_ram), MP_ROM_PTR(&rtc_ram_obj) },
};
static MP_DEFINE_CONST_DICT(rtc_globals, rtc_globals_table);
static const mp_obj_module_t bsp_rtc_module = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&rtc_globals,
};

// ---------------------------------------------------------------------------
// bsp.battery
// ---------------------------------------------------------------------------

static mp_obj_t battery_init(void) {
    check_esp_err(bsp_battery_init());
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(battery_init_obj, battery_init);

static mp_obj_t battery_voltage(void) {
    float v;
    check_esp_err(bsp_battery_voltage(&v));
    return mp_obj_new_float(v);
}
static MP_DEFINE_CONST_FUN_OBJ_0(battery_voltage_obj, battery_voltage);

// percent([volts]) -> 0..100 (measures if no voltage is given)
static mp_obj_t battery_percent(size_t n_args, const mp_obj_t *args) {
    float v;
    if (n_args == 0) {
        check_esp_err(bsp_battery_voltage(&v));
    } else {
        v = mp_obj_get_float(args[0]);
    }
    return MP_OBJ_NEW_SMALL_INT(bsp_battery_percent(v));
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(battery_percent_obj, 0, 1, battery_percent);

static const mp_rom_map_elem_t battery_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_battery) },
    { MP_ROM_QSTR(MP_QSTR_init), MP_ROM_PTR(&battery_init_obj) },
    { MP_ROM_QSTR(MP_QSTR_voltage), MP_ROM_PTR(&battery_voltage_obj) },
    { MP_ROM_QSTR(MP_QSTR_percent), MP_ROM_PTR(&battery_percent_obj) },
};
static MP_DEFINE_CONST_DICT(battery_globals, battery_globals_table);
static const mp_obj_module_t bsp_battery_module = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&battery_globals,
};

// ---------------------------------------------------------------------------
// bsp
// ---------------------------------------------------------------------------

static const mp_rom_map_elem_t bsp_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_bsp) },
    { MP_ROM_QSTR(MP_QSTR_init), MP_ROM_PTR(&bsp_init_obj) },
    { MP_ROM_QSTR(MP_QSTR_power), MP_ROM_PTR(&bsp_power_module) },
    { MP_ROM_QSTR(MP_QSTR_i2c), MP_ROM_PTR(&bsp_i2c_module) },
    { MP_ROM_QSTR(MP_QSTR_epd), MP_ROM_PTR(&bsp_epd_module) },
    { MP_ROM_QSTR(MP_QSTR_shtc3), MP_ROM_PTR(&bsp_shtc3_module) },
    { MP_ROM_QSTR(MP_QSTR_rtc), MP_ROM_PTR(&bsp_rtc_module) },
    { MP_ROM_QSTR(MP_QSTR_battery), MP_ROM_PTR(&bsp_battery_module) },
    { MP_ROM_QSTR(MP_QSTR_PIN_BOOT_BUTTON), MP_ROM_INT(BSP_PIN_BOOT_BUTTON) },
    { MP_ROM_QSTR(MP_QSTR_PIN_PWR_BUTTON), MP_ROM_INT(BSP_PIN_PWR_BUTTON) },
};
static MP_DEFINE_CONST_DICT(bsp_globals, bsp_globals_table);

const mp_obj_module_t bsp_user_cmodule = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&bsp_globals,
};

MP_REGISTER_MODULE(MP_QSTR_bsp, bsp_user_cmodule);
