#if !defined(UNIT_TEST)
#include "options.h"
#include "helpers.h"
#include "logging.h"
#include <LittleFS.h>
#include <ArduinoJson.h>

typedef enum {
    INT,
    BOOL,
    FLOAT,
    ARRAY,
    COUNT
} datatype_t;

// Every JSON key the hardware layout understands. The strings and the
// table itself live in flash (PROGMEM) because they are only walked at
// boot and when a layout is uploaded; on ESP8266 a plain const would
// otherwise cost ~3.3KB of DRAM.
#define HARDWARE_FIELDS(X) \
    X(HARDWARE_customised, "customised", BOOL) \
    X(HARDWARE_serial_rx, "serial_rx", INT) \
    X(HARDWARE_serial_tx, "serial_tx", INT) \
    X(HARDWARE_serial1_rx, "serial1_rx", INT) \
    X(HARDWARE_serial1_tx, "serial1_tx", INT) \
    X(HARDWARE_radio_busy, "radio_busy", INT) \
    X(HARDWARE_radio_busy_2, "radio_busy_2", INT) \
    X(HARDWARE_radio_dio0, "radio_dio0", INT) \
    X(HARDWARE_radio_dio0_2, "radio_dio0_2", INT) \
    X(HARDWARE_radio_dio1, "radio_dio1", INT) \
    X(HARDWARE_radio_dio1_2, "radio_dio1_2", INT) \
    X(HARDWARE_radio_miso, "radio_miso", INT) \
    X(HARDWARE_radio_mosi, "radio_mosi", INT) \
    X(HARDWARE_radio_nss, "radio_nss", INT) \
    X(HARDWARE_radio_nss_2, "radio_nss_2", INT) \
    X(HARDWARE_radio_rst, "radio_rst", INT) \
    X(HARDWARE_radio_rst_2, "radio_rst_2", INT) \
    X(HARDWARE_radio_sck, "radio_sck", INT) \
    X(HARDWARE_radio_dcdc, "radio_dcdc", BOOL) \
    X(HARDWARE_radio_rfo_hf, "radio_rfo_hf", BOOL) \
    X(HARDWARE_radio_rfsw_ctrl, "radio_rfsw_ctrl", ARRAY) \
    X(HARDWARE_radio_rfsw_ctrl_count, "radio_rfsw_ctrl", COUNT) \
    X(HARDWARE_radio_tcxo, "radio_tcxo", INT) \
    X(HARDWARE_radio_tcxo_delay, "radio_tcxo_delay", INT) \
    X(HARDWARE_ant_ctrl, "ant_ctrl", INT) \
    X(HARDWARE_ant_group, "ant_group", INT) \
    X(HARDWARE_power_enable, "power_enable", INT) \
    X(HARDWARE_power_apc2, "power_apc2", INT) \
    X(HARDWARE_power_rxen, "power_rxen", INT) \
    X(HARDWARE_power_txen, "power_txen", INT) \
    X(HARDWARE_power_rxen_2, "power_rxen_2", INT) \
    X(HARDWARE_power_txen_2, "power_txen_2", INT) \
    X(HARDWARE_power_lna_gain, "power_lna_gain", INT) \
    X(HARDWARE_power_min, "power_min", INT) \
    X(HARDWARE_power_max, "power_max", INT) \
    X(HARDWARE_power_default, "power_default", INT) \
    X(HARDWARE_power_pdet, "power_pdet", INT) \
    X(HARDWARE_power_pdet_intercept, "power_pdet_intercept", FLOAT) \
    X(HARDWARE_power_pdet_slope, "power_pdet_slope", FLOAT) \
    X(HARDWARE_power_control, "power_control", INT) \
    X(HARDWARE_power_values, "power_values", ARRAY) \
    X(HARDWARE_power_values_count, "power_values", COUNT) \
    X(HARDWARE_power_values2, "power_values2", ARRAY) \
    X(HARDWARE_power_values_dual, "power_values_dual", ARRAY) \
    X(HARDWARE_power_values_dual_count, "power_values_dual", COUNT) \
    X(HARDWARE_joystick, "joystick", INT) \
    X(HARDWARE_joystick_values, "joystick_values", ARRAY) \
    X(HARDWARE_five_way1, "five_way1", INT) \
    X(HARDWARE_five_way2, "five_way2", INT) \
    X(HARDWARE_five_way3, "five_way3", INT) \
    X(HARDWARE_button, "button", INT) \
    X(HARDWARE_button_led_index, "button_led_index", INT) \
    X(HARDWARE_button2, "button2", INT) \
    X(HARDWARE_button2_led_index, "button2_led_index", INT) \
    X(HARDWARE_led, "led", INT) \
    X(HARDWARE_led_blue, "led_blue", INT) \
    X(HARDWARE_led_blue_invert, "led_blue_invert", BOOL) \
    X(HARDWARE_led_green, "led_green", INT) \
    X(HARDWARE_led_green_invert, "led_green_invert", BOOL) \
    X(HARDWARE_led_green_red, "led_green_red", INT) \
    X(HARDWARE_led_red, "led_red", INT) \
    X(HARDWARE_led_red_invert, "led_red_invert", BOOL) \
    X(HARDWARE_led_red_green, "led_red_green", INT) \
    X(HARDWARE_led_rgb, "led_rgb", INT) \
    X(HARDWARE_led_rgb_isgrb, "led_rgb_isgrb", BOOL) \
    X(HARDWARE_ledidx_rgb_status, "ledidx_rgb_status", ARRAY) \
    X(HARDWARE_ledidx_rgb_status_count, "ledidx_rgb_status", COUNT) \
    X(HARDWARE_ledidx_rgb_vtx, "ledidx_rgb_vtx", ARRAY) \
    X(HARDWARE_ledidx_rgb_vtx_count, "ledidx_rgb_vtx", COUNT) \
    X(HARDWARE_ledidx_rgb_boot, "ledidx_rgb_boot", ARRAY) \
    X(HARDWARE_ledidx_rgb_boot_count, "ledidx_rgb_boot", COUNT) \
    X(HARDWARE_screen_cs, "screen_cs", INT) \
    X(HARDWARE_screen_dc, "screen_dc", INT) \
    X(HARDWARE_screen_mosi, "screen_mosi", INT) \
    X(HARDWARE_screen_rst, "screen_rst", INT) \
    X(HARDWARE_screen_sck, "screen_sck", INT) \
    X(HARDWARE_screen_sda, "screen_sda", INT) \
    X(HARDWARE_screen_type, "screen_type", INT) \
    X(HARDWARE_screen_reversed, "screen_reversed", BOOL) \
    X(HARDWARE_screen_mirror, "screen_mirror", BOOL) \
    X(HARDWARE_screen_bl, "screen_bl", INT) \
    X(HARDWARE_use_backpack, "use_backpack", BOOL) \
    X(HARDWARE_debug_backpack_baud, "debug_backpack_baud", INT) \
    X(HARDWARE_debug_backpack_rx, "debug_backpack_rx", INT) \
    X(HARDWARE_debug_backpack_tx, "debug_backpack_tx", INT) \
    X(HARDWARE_backpack_boot, "backpack_boot", INT) \
    X(HARDWARE_backpack_en, "backpack_en", INT) \
    X(HARDWARE_passthrough_baud, "passthrough_baud", INT) \
    X(HARDWARE_i2c_scl, "i2c_scl", INT) \
    X(HARDWARE_i2c_sda, "i2c_sda", INT) \
    X(HARDWARE_misc_gsensor_int, "misc_gsensor_int", INT) \
    X(HARDWARE_misc_buzzer, "misc_buzzer", INT) \
    X(HARDWARE_misc_fan_en, "misc_fan_en", INT) \
    X(HARDWARE_misc_fan_pwm, "misc_fan_pwm", INT) \
    X(HARDWARE_misc_fan_tacho, "misc_fan_tacho", INT) \
    X(HARDWARE_misc_fan_speeds, "misc_fan_speeds", ARRAY) \
    X(HARDWARE_misc_fan_speeds_count, "misc_fan_speeds", COUNT) \
    X(HARDWARE_gsensor_stk8xxx, "gsensor_stk8xxx", BOOL) \
    X(HARDWARE_thermal_lm75a, "thermal_lm75a", BOOL) \
    X(HARDWARE_pwm_outputs, "pwm_outputs", ARRAY) \
    X(HARDWARE_pwm_outputs_count, "pwm_outputs", COUNT) \
    X(HARDWARE_pwm_out_only, "pwm_out_only", BOOL) \
    X(HARDWARE_vbat, "vbat", INT) \
    X(HARDWARE_vbat_offset, "vbat_offset", INT) \
    X(HARDWARE_vbat_scale, "vbat_scale", INT) \
    X(HARDWARE_vbat_atten, "vbat_atten", INT) \
    X(HARDWARE_vbat_noreading, "vbat_noreading", INT) \
    X(HARDWARE_vbat_cal_min, "vbat_cal_min", INT) \
    X(HARDWARE_vbat_cal_max, "vbat_cal_max", INT) \
    X(HARDWARE_vsrc1, "vsrc1", INT) \
    X(HARDWARE_vsrc1_offset, "vsrc1_offset", INT) \
    X(HARDWARE_vsrc1_scale, "vsrc1_scale", INT) \
    X(HARDWARE_vsrc1_atten, "vsrc1_atten", INT) \
    X(HARDWARE_vsrc1_noreading, "vsrc1_noreading", INT) \
    X(HARDWARE_vsrc1_cal_min, "vsrc1_cal_min", INT) \
    X(HARDWARE_vsrc1_cal_max, "vsrc1_cal_max", INT) \
    X(HARDWARE_vsrc2, "vsrc2", INT) \
    X(HARDWARE_vsrc2_offset, "vsrc2_offset", INT) \
    X(HARDWARE_vsrc2_scale, "vsrc2_scale", INT) \
    X(HARDWARE_vsrc2_atten, "vsrc2_atten", INT) \
    X(HARDWARE_vsrc2_noreading, "vsrc2_noreading", INT) \
    X(HARDWARE_vsrc2_cal_min, "vsrc2_cal_min", INT) \
    X(HARDWARE_vsrc2_cal_max, "vsrc2_cal_max", INT) \
    X(HARDWARE_vsrc3, "vsrc3", INT) \
    X(HARDWARE_vsrc3_offset, "vsrc3_offset", INT) \
    X(HARDWARE_vsrc3_scale, "vsrc3_scale", INT) \
    X(HARDWARE_vsrc3_atten, "vsrc3_atten", INT) \
    X(HARDWARE_vsrc3_noreading, "vsrc3_noreading", INT) \
    X(HARDWARE_vsrc3_cal_min, "vsrc3_cal_min", INT) \
    X(HARDWARE_vsrc3_cal_max, "vsrc3_cal_max", INT) \
    X(HARDWARE_vtx_amp_pwm, "vtx_amp_pwm", INT) \
    X(HARDWARE_vtx_amp_vpd, "vtx_amp_vpd", INT) \
    X(HARDWARE_vtx_amp_vref, "vtx_amp_vref", INT) \
    X(HARDWARE_vtx_nss, "vtx_nss", INT) \
    X(HARDWARE_vtx_miso, "vtx_miso", INT) \
    X(HARDWARE_vtx_mosi, "vtx_mosi", INT) \
    X(HARDWARE_vtx_sck, "vtx_sck", INT) \
    X(HARDWARE_vtx_amp_vpd_25mW, "vtx_amp_vpd_25mW", ARRAY) \
    X(HARDWARE_vtx_amp_vpd_100mW, "vtx_amp_vpd_100mW", ARRAY) \
    X(HARDWARE_vtx_amp_pwm_25mW, "vtx_amp_pwm_25mW", ARRAY) \
    X(HARDWARE_vtx_amp_pwm_100mW, "vtx_amp_pwm_100mW", ARRAY)

#define X(pos, str, typ) static const char name_##pos[] PROGMEM = str;
HARDWARE_FIELDS(X)
#undef X

typedef struct {
    nameType position;
    const char *name;
    datatype_t type;
} field_t;

static const field_t fields[] PROGMEM = {
#define X(pos, str, typ) {pos, name_##pos, typ},
HARDWARE_FIELDS(X)
#undef X
};

typedef union {
    int int_value;
    bool bool_value;
    float float_value;
    int16_t *array_value;
} data_holder_t;

static data_holder_t hardware[HARDWARE_LAST];
static String builtinHardwareConfig;

String& getHardware()
{
    File file = LittleFS.open("/hardware.json", "r");
    if (!file || file.isDirectory())
    {
        if (file)
        {
            file.close();
        }
        // Try JSON at the end of the firmware
        return builtinHardwareConfig;
    }
    builtinHardwareConfig = file.readString();
    return builtinHardwareConfig;
}

static void hardware_ClearAllFields()
{
    for (size_t i = 0; i < ARRAY_SIZE(fields); i++) {
        field_t field;
        memcpy_P(&field, &fields[i], sizeof(field));
        switch (field.type) {
            case INT:
                hardware[field.position].int_value = -1;
                break;
            case BOOL:
                hardware[field.position].bool_value = false;
                break;
            case FLOAT:
                hardware[field.position].float_value = 0.0;
                break;
            case ARRAY:
                hardware[field.position].array_value = nullptr;
                break;
            case COUNT:
                hardware[field.position].int_value = 0;
                break;
        }
    }
}

static void hardware_LoadFieldsFromDoc(JsonDocument &doc)
{
    for (size_t i = 0; i < ARRAY_SIZE(fields); i++) {
        field_t field;
        memcpy_P(&field, &fields[i], sizeof(field));
        const __FlashStringHelper *name = FPSTR(field.name);
        if (doc[name].is<JsonVariant>()) {
            switch (field.type) {
                case INT:
                    hardware[field.position].int_value = doc[name];
                    break;
                case BOOL:
                    hardware[field.position].bool_value = doc[name];
                    break;
                case FLOAT:
                    hardware[field.position].float_value = doc[name];
                    break;
                case ARRAY:
                    {
                        JsonArray array = doc[name].as<JsonArray>();
                        hardware[field.position].array_value = new int16_t[array.size()];
                        copyArray(array, hardware[field.position].array_value, array.size());
                    }
                    break;
                case COUNT:
                    {
                        JsonArray array = doc[name].as<JsonArray>();
                        hardware[field.position].int_value = (int)array.size();
                    }
                    break;
            }
        }
    }
}

bool hardware_init(EspFlashStream &strmFlash)
{
    hardware_ClearAllFields();
    builtinHardwareConfig.clear();

    Stream *strmSrc;
    JsonDocument doc;
    File file = LittleFS.open("/hardware.json", "r");
    if (!file || file.isDirectory()) {
        constexpr size_t hardwareConfigOffset = ELRSOPTS_PRODUCTNAME_SIZE + ELRSOPTS_DEVICENAME_SIZE + ELRSOPTS_OPTIONS_SIZE;
        strmFlash.setPosition(hardwareConfigOffset);
        if (!options_HasStringInFlash(strmFlash))
        {
            return false;
        }

        strmSrc = &strmFlash;
    }
    else
    {
        strmSrc = &file;
    }

    DeserializationError error = deserializeJson(doc, *strmSrc);
    if (error)
    {
        return false;
    }
    serializeJson(doc, builtinHardwareConfig);

    hardware_LoadFieldsFromDoc(doc);

    return true;
}

int hardware_pin(nameType name)
{
    return hardware[name].int_value;
}

bool hardware_flag(nameType name)
{
    return hardware[name].bool_value;
}

int hardware_int(nameType name)
{
    return hardware[name].int_value;
}

float hardware_float(nameType name)
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
