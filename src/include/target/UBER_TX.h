#define TARGET_UBER_TX

#define DEVICE_NAME "UBER TX\0\0\0\0\0\0\0\0\0"
#define HARDWARE_VERSION ""

// TARGET_TX_BETAFPV_2400_MICRO_V1
// TARGET_TX_BETAFPV_2400_V1
// TARGET_TX_BETAFPV_900_V1

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

// TARGET_TX_IFLIGHT

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
#define GPIO_PIN_RCSIGNAL_RX hardware_pin("serial-rx")
#define GPIO_PIN_RCSIGNAL_TX hardware_pin("serial-tx")

// Radio
#define GPIO_PIN_BUSY hardware_pin("radio-busy")
#define GPIO_PIN_DIO0 hardware_pin("radio-dio0")
#define GPIO_PIN_DIO1 hardware_pin("radio-dio1")
#define GPIO_PIN_DIO2 hardware_pin("radio-dio2")
#define GPIO_PIN_MISO hardware_pin("radio-miso")
#define GPIO_PIN_MOSI hardware_pin("radio-mosi")
#define GPIO_PIN_NSS hardware_pin("radio-nss")
#define GPIO_PIN_RST hardware_pin("radio-rst")
#define GPIO_PIN_SCK hardware_pin("radio-sck")
#define USE_SX1280_DCDC hardware_flag("radio-dcdc")
#define USE_SX1276_RFO_HF hardware_flag("radio-rfo-hf")

// Radio Antenna
#define GPIO_PIN_ANTENNA_SELECT hardware_pin("ant-select")
#define GPIO_PIN_ANT_CTRL_1 hardware_pin("ant-ctrl1")
#define GPIO_PIN_ANT_CTRL_2 hardware_pin("ant-ctrl2")

// Radio power
#define GPIO_PIN_PA_ENABLE hardware_pin("power-enable")
// #define GPIO_PIN_RFamp_APC1 hardware_pin("power-apc1") // stm32
#define GPIO_PIN_RFamp_APC2 hardware_pin("power-apc2")
#define GPIO_PIN_RX_ENABLE hardware_pin("power-rxen")
#define GPIO_PIN_TX_ENABLE hardware_pin("power-txen")
#define MinPower (PowerLevels_e)hardware_int("power-min")
#define HighPower (PowerLevels_e)hardware_int("power-high")
#define MaxPower (PowerLevels_e)hardware_int("power-max")
#define DefaultPower (PowerLevels_e)hardware_int("power-default")

#define GPIO_PIN_PA_PDET hardware_pin("power-pdet")
#define SKY85321_PDET_INTERCEPT hardware_float("power-pdet-intercept")
#define SKY85321_PDET_SLOPE hardware_float("power-pdet-slope")
#define USE_SKY85321 hardware_flag("power-sky85321")

// default value 0 means direct!
// #define POWER_OUTPUT_ANALOG (hardware_int("power-control")==1)   // frsky only
// #define POWER_OUTPUT_DAC (hardware_int("power-control")==2)  // stm32 only
#define POWER_OUTPUT_DACWRITE (hardware_int("power-control")==3)
#define POWER_OUTPUT_FIXED hardware_int("power-fixed")
#define POWER_OUTPUT_VALUES hardware_i16_array("power-values")


// Input
#define HAS_FIVE_WAY_BUTTON

#define GPIO_PIN_JOYSTICK hardware_pin("joystick")
#define JOY_ADC_VALUES hardware_u16_array("joystick-values")

#define GPIO_PIN_FIVE_WAY_INPUT1 hardware_pin("5way1")
#define GPIO_PIN_FIVE_WAY_INPUT2 hardware_pin("5way2")
#define GPIO_PIN_FIVE_WAY_INPUT3 hardware_pin("5way3")

#define GPIO_PIN_BUTTON hardware_pin("button")

// Lighting
#define GPIO_PIN_LED hardware_pin("led")
#define GPIO_PIN_LED_BLUE hardware_pin("led-blue")
#define GPIO_PIN_LED_GREEN hardware_pin("led-green")
#define GPIO_LED_GREEN_INVERTED hardware_flag("led-green-invert")
#define GPIO_PIN_LED_GREEN_RED hardware_pin("led-green-red")
#define GPIO_PIN_LED_RED hardware_pin("led-red")
#define GPIO_LED_RED_INVERTED hardware_pin("led-red-invert")
#define GPIO_PIN_LED_RED_GREEN hardware_pin("led-reg-green")
#define GPIO_PIN_LED_WS2812 hardware_pin("led-rgb")
// #define GPIO_PIN_LED_WS2812_FAST // stm32
#define WS2812_IS_GRB hardware_flag("led-rgb-isgrb")

// OLED
#define GPIO_PIN_OLED_CS hardware_pin("oled-cs")        // SPI
#define GPIO_PIN_OLED_DC hardware_pin("oled-dc")        // SPI
#define GPIO_PIN_OLED_MOSI hardware_pin("oled-mosi")    // SPI
#define GPIO_PIN_OLED_RST hardware_pin("oled-rst")      // SPI & I2c (optional)
#define GPIO_PIN_OLED_SCK hardware_pin("oled-sck")      // clock for SPI & I2C
#define GPIO_PIN_OLED_SDA hardware_pin("oled-sda")      // I2C data

// oled-type == 0 is no oled
#define USE_OLED_I2C (hardware_int("oled-type")==1)
#define USE_OLED_SPI (hardware_int("oled-type")==2)
#define USE_OLED_SPI_SMALL (hardware_int("oled-type")==3)
#define OLED_REVERSED hardware_flag("oled-reversed")

// TFT
#define GPIO_PIN_TFT_BL 1 // hardware_pin("tft-bl")
#define GPIO_PIN_TFT_CS 2 // hardware_pin("tft-cs")
#define GPIO_PIN_TFT_DC 3 // hardware_pin("tft-dc")
#define GPIO_PIN_TFT_MOSI 4 // hardware_pin("tft-mosi")
#define GPIO_PIN_TFT_RST 5 // hardware_pin("tft-rst")
#define GPIO_PIN_TFT_SCLK 6 // hardware_pin("tft-sclk")
#define HAS_TFT_SCREEN
/*
// PWM
GPIO_PIN_PWM_OUTPUTS

// VTX
GPIO_PIN_RF_AMP_PWM
GPIO_PIN_RF_AMP_VPD
GPIO_PIN_RF_AMP_VREF
GPIO_PIN_SPI_VTX_NSS
VPD_VALUES_100MW
VPD_VALUES_25MW

// I2C
GPIO_PIN_SCL
GPIO_PIN_SDA

// Backpack
USE_TX_BACKPACK
BACKPACK_LOGGING_BAUD
GPIO_PIN_DEBUG_RX
GPIO_PIN_DEBUG_TX
GPIO_PIN_BACKPACK_BOOT
GPIO_PIN_BACKPACK_EN
*/

// Misc sensors & things
#define GPIO_PIN_GSENSOR_INT hardware_pin("misc-gsensor-int")
// #define GPIO_PIN_BUZZER hardware_pin("misc-buzzer")  // stm32 only
#define GPIO_PIN_FAN_EN hardware_pin("misc-fan-en")

#define HAS_GSENSOR_STK8xxx
#define HAS_GSENSOR

#define HAS_THERMAL_LM75A
#define HAS_THERMAL
