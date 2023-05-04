#include <stdint.h>

typedef enum {
    // Serial
    HARDWARE_serial_rx,
    HARDWARE_serial_tx,

    // Radio
    HARDWARE_radio_busy,
    HARDWARE_radio_busy_2,
    HARDWARE_radio_dio0,
    HARDWARE_radio_dio0_2,
    HARDWARE_radio_dio1,
    HARDWARE_radio_dio1_2,
    HARDWARE_radio_dio2,
    HARDWARE_radio_miso,
    HARDWARE_radio_mosi,
    HARDWARE_radio_nss,
    HARDWARE_radio_nss_2,
    HARDWARE_radio_rst,
    HARDWARE_radio_sck,
    HARDWARE_radio_dcdc,
    HARDWARE_radio_rfo_hf,

    // Radio Antenna
    HARDWARE_ant_ctrl,
    HARDWARE_ant_ctrl_compl,

    // Radio power
    HARDWARE_power_enable,
    HARDWARE_power_apc1, // stm32
    HARDWARE_power_apc2,
    HARDWARE_power_rxen,
    HARDWARE_power_txen,
    HARDWARE_power_rxen_2,
    HARDWARE_power_txen_2,
    HARDWARE_power_lna_gain,
    HARDWARE_power_min,
    HARDWARE_power_high,
    HARDWARE_power_max,
    HARDWARE_power_default,

    HARDWARE_power_pdet,
    HARDWARE_power_pdet_intercept,
    HARDWARE_power_pdet_slope,

    HARDWARE_power_control,
    HARDWARE_power_values,

    // Input
    HARDWARE_joystick,
    HARDWARE_joystick_values,

    HARDWARE_five_way1,
    HARDWARE_five_way2,
    HARDWARE_five_way3,

    HARDWARE_button,
    HARDWARE_button_led_index,
    HARDWARE_button2,
    HARDWARE_button2_led_index,

    // Lighting
    HARDWARE_led,
    HARDWARE_led_blue,
    HARDWARE_led_blue_invert,
    HARDWARE_led_green,
    HARDWARE_led_green_invert,
    HARDWARE_led_green_red,
    HARDWARE_led_red,
    HARDWARE_led_red_invert,
    HARDWARE_led_reg_green,
    HARDWARE_led_rgb,
    // GPIO_PIN_LED_WS2812_FAST // stm32
    HARDWARE_led_rgb_isgrb,
    HARDWARE_ledidx_rgb_status,
    HARDWARE_ledidx_rgb_status_count,
    HARDWARE_ledidx_rgb_vtx,
    HARDWARE_ledidx_rgb_vtx_count,
    HARDWARE_ledidx_rgb_boot,
    HARDWARE_ledidx_rgb_boot_count,

    // OLED/TFT
    HARDWARE_screen_cs,       // SPI
    HARDWARE_screen_dc,       // SPI
    HARDWARE_screen_mosi,     // SPI
    HARDWARE_screen_rst,      // SPI & I2c (optional)
    HARDWARE_screen_sck,      // clock for SPI & I2C
    HARDWARE_screen_sda,      // I2C data

    // screen_type == 0 is no screen
    HARDWARE_screen_type,
    HARDWARE_screen_reversed,
    HARDWARE_screen_bl,

    // Backpack
    HARDWARE_use_backpack,
    HARDWARE_debug_backpack_baud,
    HARDWARE_debug_backpack_rx,
    HARDWARE_debug_backpack_tx,
    HARDWARE_backpack_boot,
    HARDWARE_backpack_en,
    HARDWARE_passthrough_baud,

    // I2C & Misc devices
    HARDWARE_i2c_scl,
    HARDWARE_i2c_sda,
    HARDWARE_misc_gsensor_int,
    HARDWARE_misc_buzzer,  // stm32 only
    HARDWARE_misc_fan_en,
    HARDWARE_misc_fan_pwm,
    HARDWARE_misc_fan_tacho,
    HARDWARE_misc_fan_speeds,
    HARDWARE_misc_fan_speeds_count,
    HARDWARE_gsensor_stk8xxx,
    HARDWARE_thermal_lm75a,

    // PWM
    HARDWARE_pwm_outputs,
    HARDWARE_pwm_outputs_count,

    // VBAT
    HARDWARE_vbat,
    HARDWARE_vbat_offset,
    HARDWARE_vbat_scale,

    // VTX
    HARDWARE_vtx_amp_pwm,
    HARDWARE_vtx_amp_vpd,
    HARDWARE_vtx_amp_vref,
    HARDWARE_vtx_nss,
    HARDWARE_vtx_miso,
    HARDWARE_vtx_mosi,
    HARDWARE_vtx_sck,
    HARDWARE_vtx_amp_vpd_25mW,
    HARDWARE_vtx_amp_vpd_100mW,

    HARDWARE_LAST
} nameType;

bool hardware_init();
const int hardware_pin(nameType name);
const bool hardware_flag(nameType name);
const int hardware_int(nameType name);
const float hardware_float(nameType name);
const int16_t* hardware_i16_array(nameType name);
const uint16_t* hardware_u16_array(nameType name);
