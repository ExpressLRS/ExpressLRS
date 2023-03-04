#define TARGET_UNIFIED_TX

#define HARDWARE_VERSION ""
// DEVICE_NAME is not defined here because we get it from the SPIFFS file system

// // FM30
// TARGET_TX_FM30
// GPIO_PIN_ANT_CTRL
// GPIO_PIN_BLUETOOTH_EN
// GPIO_PIN_UART1RX_INVERT
// GPIO_PIN_UART1TX_INVERT
// GPIO_PIN_UART3RX_INVERT

// // Ghost
// TARGET_RX_GHOST_ATTO_V1
// TARGET_TX_GHOST
// TARGET_TX_GHOST_LITE
// GPIO_PIN_RF_AMP_DET         // unused

// // Frsky/HM915
// GPIO_PIN_VRF1               // unused
// GPIO_PIN_VRF2               // unused
// GPIO_PIN_SWR                // unused
// TARGET_EEPROM_400K
// TARGET_EEPROM_ADDR
// TARGET_USE_EEPROM

// // various (unused)
// GPIO_PIN_DIP1
// GPIO_PIN_DIP2
// GPIO_PIN_DIP3
// GPIO_PIN_DIP4

//
// ====================================
//

// Serial
#define GPIO_PIN_RCSIGNAL_RX hardware_pin(HARDWARE_serial_rx)
#define GPIO_PIN_RCSIGNAL_TX hardware_pin(HARDWARE_serial_tx)

// Radio
#define GPIO_PIN_BUSY hardware_pin(HARDWARE_radio_busy)
#define GPIO_PIN_BUSY_2 hardware_pin(HARDWARE_radio_busy_2)
#define GPIO_PIN_DIO0 hardware_pin(HARDWARE_radio_dio0)
#define GPIO_PIN_DIO0_2 hardware_pin(HARDWARE_radio_dio0_2)
#define GPIO_PIN_DIO1 hardware_pin(HARDWARE_radio_dio1)
#define GPIO_PIN_DIO1_2 hardware_pin(HARDWARE_radio_dio1_2)
#define GPIO_PIN_DIO2 hardware_pin(HARDWARE_radio_dio2)
#define GPIO_PIN_MISO hardware_pin(HARDWARE_radio_miso)
#define GPIO_PIN_MOSI hardware_pin(HARDWARE_radio_mosi)
#define GPIO_PIN_NSS hardware_pin(HARDWARE_radio_nss)
#define GPIO_PIN_NSS_2 hardware_pin(HARDWARE_radio_nss_2)
#define GPIO_PIN_RST hardware_pin(HARDWARE_radio_rst)
#define GPIO_PIN_SCK hardware_pin(HARDWARE_radio_sck)
#define USE_SX1280_DCDC
#define OPT_USE_SX1280_DCDC hardware_flag(HARDWARE_radio_dcdc)
#define USE_SX1276_RFO_HF
#define OPT_USE_SX1276_RFO_HF hardware_flag(HARDWARE_radio_rfo_hf)

// Radio Antenna
#define GPIO_PIN_ANT_CTRL hardware_pin(HARDWARE_ant_ctrl)
#define GPIO_PIN_ANT_CTRL_COMPL hardware_pin(HARDWARE_ant_ctrl_compl)

// Radio power
#define GPIO_PIN_PA_ENABLE hardware_pin(HARDWARE_power_enable)
// #define GPIO_PIN_RFamp_APC1 hardware_pin(HARDWARE_power_apc1) // stm32
#define GPIO_PIN_RFamp_APC2 hardware_pin(HARDWARE_power_apc2)
#define GPIO_PIN_RX_ENABLE hardware_pin(HARDWARE_power_rxen)
#define GPIO_PIN_TX_ENABLE hardware_pin(HARDWARE_power_txen)
#define GPIO_PIN_RX_ENABLE_2 hardware_pin(HARDWARE_power_rxen_2)
#define GPIO_PIN_TX_ENABLE_2 hardware_pin(HARDWARE_power_txen_2)
#define LBT_RSSI_THRESHOLD_OFFSET_DB hardware_int(HARDWARE_power_lna_gain)
#define MinPower (PowerLevels_e)hardware_int(HARDWARE_power_min)
#define HighPower (PowerLevels_e)hardware_int(HARDWARE_power_high)
#define MaxPower (PowerLevels_e)hardware_int(HARDWARE_power_max)
#define DefaultPower (PowerLevels_e)hardware_int(HARDWARE_power_default)

#define USE_SKY85321
#define GPIO_PIN_PA_PDET hardware_pin(HARDWARE_power_pdet)
#define SKY85321_PDET_INTERCEPT hardware_float(HARDWARE_power_pdet_intercept)
#define SKY85321_PDET_SLOPE hardware_float(HARDWARE_power_pdet_slope)

// default value 0 means direct!
// #define POWER_OUTPUT_ANALOG (hardware_int(HARDWARE_power_control)==1)   // frsky only
// #define POWER_OUTPUT_DAC (hardware_int(HARDWARE_power_control)==2)  // stm32 only
#define POWER_OUTPUT_DACWRITE (hardware_int(HARDWARE_power_control)==3)
#define POWER_OUTPUT_FIXED -99
#define POWER_OUTPUT_VALUES hardware_i16_array(HARDWARE_power_values)

// Input
#define HAS_FIVE_WAY_BUTTON

#define GPIO_PIN_JOYSTICK hardware_pin(HARDWARE_joystick)
#define JOY_ADC_VALUES hardware_u16_array(HARDWARE_joystick_values)

#define GPIO_PIN_FIVE_WAY_INPUT1 hardware_pin(HARDWARE_five_way1)
#define GPIO_PIN_FIVE_WAY_INPUT2 hardware_pin(HARDWARE_five_way2)
#define GPIO_PIN_FIVE_WAY_INPUT3 hardware_pin(HARDWARE_five_way3)

#define GPIO_PIN_BUTTON hardware_pin(HARDWARE_button)
#define USER_BUTTON_LED hardware_pin(HARDWARE_button_led_index)
#define GPIO_PIN_BUTTON2 hardware_pin(HARDWARE_button2)
#define USER_BUTTON2_LED hardware_pin(HARDWARE_button2_led_index)

// Lighting
#define GPIO_PIN_LED hardware_pin(HARDWARE_led)
#define GPIO_PIN_LED_BLUE hardware_pin(HARDWARE_led_blue)
#define GPIO_LED_BLUE_INVERTED hardware_pin(HARDWARE_led_blue_invert)
#define GPIO_PIN_LED_GREEN hardware_pin(HARDWARE_led_green)
#define GPIO_LED_GREEN_INVERTED hardware_flag(HARDWARE_led_green_invert)
#define GPIO_PIN_LED_GREEN_RED hardware_pin(HARDWARE_led_green_red)
#define GPIO_PIN_LED_RED hardware_pin(HARDWARE_led_red)
#define GPIO_LED_RED_INVERTED hardware_pin(HARDWARE_led_red_invert)
#define GPIO_PIN_LED_RED_GREEN hardware_pin(HARDWARE_led_reg_green)

#define GPIO_PIN_LED_WS2812 hardware_pin(HARDWARE_led_rgb)
// #define GPIO_PIN_LED_WS2812_FAST // stm32
#define WS2812_IS_GRB
#define OPT_WS2812_IS_GRB hardware_flag(HARDWARE_led_rgb_isgrb)
#define WS2812_STATUS_LEDS hardware_i16_array(HARDWARE_ledidx_rgb_status)
#define WS2812_STATUS_LEDS_COUNT hardware_int(HARDWARE_ledidx_rgb_status_count)
#define WS2812_VTX_STATUS_LEDS hardware_i16_array(HARDWARE_ledidx_rgb_vtx)
#define WS2812_VTX_STATUS_LEDS_COUNT hardware_int(HARDWARE_ledidx_rgb_vtx_count)
#define WS2812_BOOT_LEDS hardware_i16_array(HARDWARE_ledidx_rgb_boot)
#define WS2812_BOOT_LEDS_COUNT hardware_int(HARDWARE_ledidx_rgb_boot_count)

// OLED
#define GPIO_PIN_OLED_CS hardware_pin(HARDWARE_screen_cs)        // SPI
#define GPIO_PIN_OLED_DC hardware_pin(HARDWARE_screen_dc)        // SPI
#define GPIO_PIN_OLED_MOSI hardware_pin(HARDWARE_screen_mosi)    // SPI
#define GPIO_PIN_OLED_RST hardware_pin(HARDWARE_screen_rst)      // SPI & I2c (optional)
#define GPIO_PIN_OLED_SCK hardware_pin(HARDWARE_screen_sck)      // clock for SPI & I2C
#define GPIO_PIN_OLED_SDA hardware_pin(HARDWARE_screen_sda)      // I2C data

// screen_type == 0 is no oled
#define USE_OLED_I2C
#define OPT_USE_OLED_I2C (hardware_int(HARDWARE_screen_type)==1)
#define USE_OLED_SPI
#define OPT_USE_OLED_SPI (hardware_int(HARDWARE_screen_type)==2)
#define USE_OLED_SPI_SMALL
#define OPT_USE_OLED_SPI_SMALL (hardware_int(HARDWARE_screen_type)==3)
#define OLED_REVERSED
#define OPT_OLED_REVERSED hardware_flag(HARDWARE_screen_reversed)

// TFT
#define HAS_TFT_SCREEN
#define OPT_HAS_TFT_SCREEN (hardware_int(HARDWARE_screen_type)==4)

#define GPIO_PIN_TFT_BL hardware_pin(HARDWARE_screen_bl)
#define GPIO_PIN_TFT_CS hardware_pin(HARDWARE_screen_cs)
#define GPIO_PIN_TFT_DC hardware_pin(HARDWARE_screen_dc)
#define GPIO_PIN_TFT_MOSI hardware_pin(HARDWARE_screen_mosi)
#define GPIO_PIN_TFT_RST hardware_pin(HARDWARE_screen_rst)
#define GPIO_PIN_TFT_SCLK hardware_pin(HARDWARE_screen_sck)

// Backpack
#define USE_TX_BACKPACK
#define OPT_USE_TX_BACKPACK hardware_flag(HARDWARE_use_backpack)
#define BACKPACK_LOGGING_BAUD hardware_int(HARDWARE_debug_backpack_baud)
#define GPIO_PIN_DEBUG_RX hardware_pin(HARDWARE_debug_backpack_rx)
#define GPIO_PIN_DEBUG_TX hardware_pin(HARDWARE_debug_backpack_tx)
#define GPIO_PIN_BACKPACK_BOOT hardware_pin(HARDWARE_backpack_boot)
#define GPIO_PIN_BACKPACK_EN hardware_pin(HARDWARE_backpack_en)
#define PASSTHROUGH_BAUD hardware_int(HARDWARE_passthrough_baud)

// I2C
#define USE_I2C
#define GPIO_PIN_SCL hardware_pin(HARDWARE_i2c_scl)
#define GPIO_PIN_SDA hardware_pin(HARDWARE_i2c_sda)

// Misc sensors & things
#define GPIO_PIN_GSENSOR_INT hardware_pin(HARDWARE_misc_gsensor_int)
// #define GPIO_PIN_BUZZER hardware_pin(HARDWARE_misc_buzzer)  // stm32 only
#define define HAS_FAN
#define GPIO_PIN_FAN_EN hardware_pin(HARDWARE_misc_fan_en)
#define GPIO_PIN_FAN_PWM hardware_pin(HARDWARE_misc_fan_pwm)
#define GPIO_PIN_FAN_TACHO hardware_pin(HARDWARE_misc_fan_tacho)
#define GPIO_PIN_FAN_SPEEDS hardware_u16_array(HARDWARE_misc_fan_speeds)
#define GPIO_PIN_FAN_SPEEDS_COUNT hardware_int(HARDWARE_misc_fan_speeds_count)

#define HAS_GSENSOR
#define HAS_GSENSOR_STK8xxx
#define OPT_HAS_GSENSOR_STK8xxx hardware_flag(HARDWARE_gsensor_stk8xxx)
#define OPT_HAS_GSENSOR OPT_HAS_GSENSOR_STK8xxx // If any of the sensors are present

#define HAS_THERMAL
#define HAS_THERMAL_LM75A
#define OPT_HAS_THERMAL_LM75A hardware_flag(HARDWARE_thermal_lm75a)
#define OPT_HAS_THERMAL OPT_HAS_THERMAL_LM75A // If any of the sensors are present

/*
// These are RX settings

// PWM
GPIO_PIN_PWM_OUTPUTS

// VTX
GPIO_PIN_RF_AMP_PWM
GPIO_PIN_RF_AMP_VPD
GPIO_PIN_RF_AMP_VREF
GPIO_PIN_SPI_VTX_NSS
VPD_VALUES_100MW
VPD_VALUES_25MW
*/
