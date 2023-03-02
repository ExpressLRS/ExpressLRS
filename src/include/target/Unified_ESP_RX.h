#define TARGET_UNIFIED_RX

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
#define GPIO_PIN_BUTTON hardware_pin(HARDWARE_button)

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

// I2C
#define GPIO_PIN_SCL hardware_pin(HARDWARE_i2c_scl)
#define GPIO_PIN_SDA hardware_pin(HARDWARE_i2c_sda)

// PWM
#define GPIO_PIN_PWM_OUTPUTS hardware_i16_array(HARDWARE_pwm_outputs)
#define GPIO_PIN_PWM_OUTPUTS_COUNT hardware_int(HARDWARE_pwm_outputs_count)

// VBat
#define USE_ANALOG_VBAT
#define GPIO_ANALOG_VBAT hardware_pin(HARDWARE_vbat)
#define ANALOG_VBAT_OFFSET hardware_int(HARDWARE_vbat_offset)
#define ANALOG_VBAT_SCALE hardware_int(HARDWARE_vbat_scale)

// VTX
#define HAS_VTX_SPI
#define OPT_HAS_VTX_SPI (hardware_pin(HARDWARE_vtx_nss) != UNDEF_PIN)
#define GPIO_PIN_RF_AMP_PWM hardware_pin(HARDWARE_vtx_amp_pwm)
#define GPIO_PIN_RF_AMP_VPD hardware_pin(HARDWARE_vtx_amp_vpd)
#define GPIO_PIN_RF_AMP_VREF hardware_pin(HARDWARE_vtx_amp_vref)
#define GPIO_PIN_SPI_VTX_NSS hardware_pin(HARDWARE_vtx_nss)
#define GPIO_PIN_SPI_VTX_MISO hardware_pin(HARDWARE_vtx_miso)
#define GPIO_PIN_SPI_VTX_MOSI hardware_pin(HARDWARE_vtx_mosi)
#define GPIO_PIN_SPI_VTX_SCK hardware_pin(HARDWARE_vtx_sck)
#define VPD_VALUES_25MW hardware_u16_array(HARDWARE_vtx_amp_vpd_25mW)
#define VPD_VALUES_100MW hardware_u16_array(HARDWARE_vtx_amp_vpd_100mW)
