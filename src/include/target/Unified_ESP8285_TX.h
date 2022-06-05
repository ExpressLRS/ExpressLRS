#define TARGET_UNIFIED_TX

#define HARDWARE_VERSION ""

// DEVICE_NAME is not defined here because we get it from the SPIFFS file system
// Serial
#define GPIO_PIN_RCSIGNAL_RX hardware_pin(HARDWARE_serial_rx)
#define GPIO_PIN_RCSIGNAL_TX hardware_pin(HARDWARE_serial_tx)

// Radio
#define GPIO_PIN_BUSY hardware_pin(HARDWARE_radio_busy)
#define GPIO_PIN_BUSY_2 hardware_pin(HARDWARE_radio_busy_2)
#define GPIO_PIN_DIO0 hardware_pin(HARDWARE_radio_dio0)
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

// Radio power
#define GPIO_PIN_PA_ENABLE hardware_pin(HARDWARE_power_enable)
// #define GPIO_PIN_RFamp_APC1 hardware_pin(HARDWARE_power_apc1) // stm32
#define GPIO_PIN_RFamp_APC2 hardware_pin(HARDWARE_power_apc2)
#define GPIO_PIN_RX_ENABLE hardware_pin(HARDWARE_power_rxen)
#define GPIO_PIN_TX_ENABLE hardware_pin(HARDWARE_power_txen)
#define MinPower (PowerLevels_e)hardware_int(HARDWARE_power_min)
#define HighPower (PowerLevels_e)hardware_int(HARDWARE_power_high)
#define MaxPower (PowerLevels_e)hardware_int(HARDWARE_power_max)
#define DefaultPower (PowerLevels_e)hardware_int(HARDWARE_power_default)

//#define POWER_OUTPUT_DACWRITE (hardware_int(HARDWARE_power_control)==3)
#define POWER_OUTPUT_FIXED -99
#define POWER_OUTPUT_VALUES hardware_i16_array(HARDWARE_power_values)

// Input
#define GPIO_PIN_BUTTON hardware_pin(HARDWARE_button)

// Lighting
#define GPIO_PIN_LED_RED hardware_pin(HARDWARE_led_red)
#define GPIO_LED_RED_INVERTED hardware_pin(HARDWARE_led_red_invert)
// No NeoPixelBus for ESP8285 (uses global Serial)
//#define GPIO_PIN_LED_WS2812 hardware_pin(HARDWARE_led_rgb)
//#define WS2812_IS_GRB
//#define OPT_WS2812_IS_GRB hardware_flag(HARDWARE_led_rgb_isgrb)

#define GPIO_PIN_FAN_EN hardware_pin(HARDWARE_misc_fan_en)

