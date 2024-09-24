#define HAS_BARO

// Serial
#define GPIO_PIN_RCSIGNAL_RX hardware_pin(HARDWARE_serial_rx)
#define GPIO_PIN_RCSIGNAL_TX hardware_pin(HARDWARE_serial_tx)
#define GPIO_PIN_SERIAL1_RX hardware_pin(HARDWARE_serial1_rx)
#define GPIO_PIN_SERIAL1_TX hardware_pin(HARDWARE_serial1_tx)

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
#define GPIO_PIN_RST_2 hardware_pin(HARDWARE_radio_rst_2)
#define GPIO_PIN_SCK hardware_pin(HARDWARE_radio_sck)
#define USE_HARDWARE_DCDC
#define OPT_USE_HARDWARE_DCDC hardware_flag(HARDWARE_radio_dcdc)
#define USE_SX1276_RFO_HF
#define OPT_USE_SX1276_RFO_HF hardware_flag(HARDWARE_radio_rfo_hf)
#define LR1121_RFSW_CTRL hardware_u16_array(HARDWARE_radio_rfsw_ctrl)
#define LR1121_RFSW_CTRL_COUNT hardware_int(HARDWARE_radio_rfsw_ctrl_count)

// Radio Antenna
#define GPIO_PIN_ANT_CTRL hardware_pin(HARDWARE_ant_ctrl)
#define GPIO_PIN_ANT_CTRL_COMPL hardware_pin(HARDWARE_ant_ctrl_compl)

// Radio power
#define GPIO_PIN_PA_ENABLE hardware_pin(HARDWARE_power_enable)
#define GPIO_PIN_RFamp_APC1 hardware_pin(HARDWARE_power_apc1)
#define GPIO_PIN_RFamp_APC2 hardware_pin(HARDWARE_power_apc2)
#define GPIO_PIN_RX_ENABLE hardware_pin(HARDWARE_power_rxen)
#define GPIO_PIN_TX_ENABLE hardware_pin(HARDWARE_power_txen)
#define GPIO_PIN_RX_ENABLE_2 hardware_pin(HARDWARE_power_rxen_2)
#define GPIO_PIN_TX_ENABLE_2 hardware_pin(HARDWARE_power_txen_2)
#define LBT_RSSI_THRESHOLD_OFFSET_DB hardware_int(HARDWARE_power_lna_gain)
#define MinPower (PowerLevels_e)hardware_int(HARDWARE_power_min)
#define MaxPower (PowerLevels_e)hardware_int(HARDWARE_power_max)
#define DefaultPower (PowerLevels_e)hardware_int(HARDWARE_power_default)

// default value 0 means direct!
#define POWER_OUTPUT_DACWRITE (hardware_int(HARDWARE_power_control)==3)
#define POWER_OUTPUT_VALUES hardware_i16_array(HARDWARE_power_values)
#define POWER_OUTPUT_VALUES2 hardware_i16_array(HARDWARE_power_values2)
#define POWER_OUTPUT_VALUES_DUAL hardware_i16_array(HARDWARE_power_values_dual)

// Input
#define GPIO_PIN_BUTTON hardware_pin(HARDWARE_button)
#define GPIO_PIN_BUTTON2 UNDEF_PIN

// Lighting
#define GPIO_PIN_LED_RED hardware_pin(HARDWARE_led)
#define GPIO_LED_RED_INVERTED hardware_pin(HARDWARE_led_red_invert)
#define GPIO_PIN_LED_BLUE UNDEF_PIN
#define GPIO_LED_BLUE_INVERTED false
#define GPIO_PIN_LED_GREEN UNDEF_PIN
#define GPIO_LED_GREEN_INVERTED false

#define GPIO_PIN_LED_WS2812 hardware_pin(HARDWARE_led_rgb)
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

#if defined(PLATFORM_ESP32)
// VTX
#define HAS_VTX_SPI
#define HAS_MSP_VTX
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
#define PWM_VALUES_25MW hardware_u16_array(HARDWARE_vtx_amp_pwm_25mW)
#define PWM_VALUES_100MW hardware_u16_array(HARDWARE_vtx_amp_pwm_100mW)
#endif

#define GPIO_PIN_FAN_EN hardware_pin(HARDWARE_misc_fan_en)

#define OPT_USE_TX_BACKPACK false
