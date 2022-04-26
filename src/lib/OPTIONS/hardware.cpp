#if defined(TARGET_UBER_TX) || defined(TARGET_UBER_RX)

#include "targets.h"
#include "helpers.h"
#include "logging.h"
#if defined(TARGET_UBER_RX)
#include <FS.h>
#else
#include <SPIFFS.h>
#endif
#include <ArduinoJson.h>

typedef enum {
    INT,
    BOOL,
    FLOAT,
    ARRAY
} datatype_t;

static const struct {
    const nameType position;
    const char *name;
    const datatype_t type;
} fields[] = {
    {HARDWARE_serial_rx, "serial_rx", INT},
    {HARDWARE_serial_tx, "serial_tx", INT},
    {HARDWARE_radio_busy, "radio_busy", INT},
    {HARDWARE_radio_dio0, "radio_dio0", INT},
    {HARDWARE_radio_dio1, "radio_dio1", INT},
    {HARDWARE_radio_dio2, "radio_dio2", INT},
    {HARDWARE_radio_miso, "radio_miso", INT},
    {HARDWARE_radio_mosi, "radio_mosi", INT},
    {HARDWARE_radio_nss, "radio_nss", INT},
    {HARDWARE_radio_rst, "radio_rst", INT},
    {HARDWARE_radio_sck, "radio_sck", INT},
    {HARDWARE_radio_dcdc, "radio_dcdc", BOOL},
    {HARDWARE_radio_rfo_hf, "radio_rfo_hf", BOOL},
    {HARDWARE_ant_select, "ant_select", INT},
    {HARDWARE_ant_ctrl1, "ant_ctrl1", INT},
    {HARDWARE_ant_ctrl2, "ant_ctrl2", INT},
    {HARDWARE_power_enable, "power_enable", INT},
    {HARDWARE_power_apc1, "power_apc1", INT},
    {HARDWARE_power_apc2, "power_apc2", INT},
    {HARDWARE_power_rxen, "power_rxen", INT},
    {HARDWARE_power_txen, "power_txen", INT},
    {HARDWARE_power_min, "power_min", INT},
    {HARDWARE_power_high, "power_high", INT},
    {HARDWARE_power_max, "power_max", INT},
    {HARDWARE_power_default, "power_default", INT},
    {HARDWARE_power_pdet, "power_pdet", INT},
    {HARDWARE_power_pdet_intercept, "power_pdet_intercept", FLOAT},
    {HARDWARE_power_pdet_slope, "power_pdet_slope", FLOAT},
    {HARDWARE_power_control, "power_control", INT},
    {HARDWARE_power_values, "power_values", ARRAY},
    {HARDWARE_joystick, "joystick", INT},
    {HARDWARE_joystick_values, "joystick_values", ARRAY},
    {HARDWARE_five_way1, "five_way1", INT},
    {HARDWARE_five_way2, "five_way2", INT},
    {HARDWARE_five_way3, "five_way3", INT},
    {HARDWARE_button, "button", INT},
    {HARDWARE_led, "led", INT},
    {HARDWARE_led_blue, "led_blue", INT},
    {HARDWARE_led_green, "led_green", INT},
    {HARDWARE_led_green_invert, "led_green_invert", BOOL},
    {HARDWARE_led_green_red, "led_green_red", INT},
    {HARDWARE_led_red, "led_red", INT},
    {HARDWARE_led_red_invert, "led_red_invert", BOOL},
    {HARDWARE_led_reg_green, "led_red_green", INT},
    {HARDWARE_led_rgb, "led_rgb", INT},
    {HARDWARE_led_rgb_isgrb, "led_rgb_isgrb", BOOL},
    {HARDWARE_screen_cs, "screen_cs", INT},
    {HARDWARE_screen_dc, "screen_dc", INT},
    {HARDWARE_screen_mosi, "screen_mosi", INT},
    {HARDWARE_screen_rst, "screen_rst", INT},
    {HARDWARE_screen_sck, "screen_sck", INT},
    {HARDWARE_screen_sda, "screen_sda", INT},
    {HARDWARE_screen_type, "screen_type", INT},
    {HARDWARE_screen_reversed, "screen_reversed", BOOL},
    {HARDWARE_screen_bl, "screen_bl", INT},
    {HARDWARE_use_backpack, "use_backpack", BOOL},
    {HARDWARE_debug_backpack_baud, "debug_backpack_baud", INT},
    {HARDWARE_debug_backpack_rx, "debug_backpack_rx", INT},
    {HARDWARE_debug_backpack_tx, "debug_backpack_tx", INT},
    {HARDWARE_backpack_boot, "backpack_boot", INT},
    {HARDWARE_backpack_en, "backpack_en", INT},
    {HARDWARE_i2c_scl, "i2c_scl", INT},
    {HARDWARE_i2c_sda, "i2c_sda", INT},
    {HARDWARE_misc_gsensor_int, "misc_gsensor_int", INT},
    {HARDWARE_misc_buzzer, "misc_buzzer", INT},
    {HARDWARE_misc_fan_en, "misc_fan_en", INT},
    {HARDWARE_gsensor_stk8xxx, "gsensor_stk8xxx", BOOL},
    {HARDWARE_thermal_lm75a, "thermal_lm75a", BOOL},
    {HARDWARE_pwm_outputs, "pwm_outputs", ARRAY},
    {HARDWARE_vbat, "vbat", INT},
    {HARDWARE_vbat_offset, "vbat_offset", INT},
    {HARDWARE_vbat_scale, "vbat_scale", INT},
    {HARDWARE_rf_amp_pwm, "rf_amp_pwm", INT},
    {HARDWARE_rf_amp_vpd, "rf_amp_vpd", INT},
    {HARDWARE_rf_amp_vref, "rf_amp_vref", INT},
    {HARDWARE_spi_vtx_nss, "spi_vtx_nss", INT},
    {HARDWARE_vpd_25mW, "vpd_25mW", ARRAY},
    {HARDWARE_vpd_100mW, "vpd_100mW", ARRAY},
};

typedef union {
    int int_value;
    bool bool_value;
    float float_value;
    int16_t *array_value;
} data_holder_t;

static data_holder_t hardware[HARDWARE_LAST];

bool hardware_init(uint32_t *config)
{
    for (size_t i=0 ; i<ARRAY_SIZE(fields) ; i++) {
        switch (fields[i].type) {
            case INT:
                hardware[fields[i].position].int_value = -1;
                break;
            case BOOL:
                hardware[fields[i].position].bool_value = false;
                break;
            case FLOAT:
                hardware[fields[i].position].float_value = 0.0;
                break;
            case ARRAY:
                hardware[fields[i].position].array_value = nullptr;
                break;
        }
    }

    DynamicJsonDocument doc(2048);
    File file = SPIFFS.open("/hardware.json", "r");
    if (!file || file.isDirectory()) {
        if (file)
        {
            file.close();
        }
        if (config[0] == 0xFFFFFFFF)
        {
            return false;
        }
        DeserializationError error = deserializeJson(doc, ((const char *)config) + 16 + 512);
        if (error) {
            return false;
        }
    }
    else
    {
        DeserializationError error = deserializeJson(doc, file);
        file.close();
        if (error) {
            return false;
        }
    }

    for (size_t i=0 ; i<ARRAY_SIZE(fields) ; i++) {
        if (doc.containsKey(fields[i].name)) {
            switch (fields[i].type) {
                case INT:
                    hardware[fields[i].position].int_value = doc[fields[i].name];
                    break;
                case BOOL:
                    hardware[fields[i].position].bool_value = doc[fields[i].name];
                    break;
                case FLOAT:
                    hardware[fields[i].position].float_value = doc[fields[i].name];
                    break;
                case ARRAY:
                    JsonArray array = doc[fields[i].name].as<JsonArray>();
                    hardware[fields[i].position].array_value = new int16_t[array.size()];
                    copyArray(doc[fields[i].name], hardware[fields[i].position].array_value, array.size());
                    break;
            }
        }
    }

    return true;
}

const int hardware_pin(nameType name)
{
    return hardware[name].int_value;
}

const bool hardware_flag(nameType name)
{
    return hardware[name].bool_value;
}

const int hardware_int(nameType name)
{
    return hardware[name].int_value;
}

const float hardware_float(nameType name)
{
    return hardware[name].float_value;
}

const int16_t* hardware_i16_array(nameType name)
{
    return hardware[name].array_value;
}

const uint16_t* hardware_u16_array(nameType name)
{
    return (uint16_t *)hardware[name].array_value;
}

#endif