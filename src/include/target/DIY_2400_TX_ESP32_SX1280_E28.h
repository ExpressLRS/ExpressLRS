#ifndef DEVICE_NAME
#define DEVICE_NAME "DIY2400 E28"
#endif

//#define HAS_TFT_SCREEN
//#define HAS_FIVE_WAY_BUTTON
// Any device features
#if !defined(USE_OLED_I2C)&&!defined(HAS_TFT_SCREEN)
#define USE_OLED_SPI
#endif
#define USE_SX1280_DCDC

// GPIO pin definitions
#define GPIO_PIN_NSS            5
#define GPIO_PIN_BUSY           21
#define GPIO_PIN_DIO1           4
#define GPIO_PIN_MOSI           23
#define GPIO_PIN_MISO           19
#define GPIO_PIN_SCK            18
#define GPIO_PIN_RST            14
#define GPIO_PIN_RX_ENABLE      27
#define GPIO_PIN_TX_ENABLE      26
#ifdef USE_OLED_SPI
#define GPIO_PIN_OLED_CS        2
#define GPIO_PIN_OLED_RST       16
#define GPIO_PIN_OLED_DC        22
#define GPIO_PIN_OLED_MOSI      32
#define GPIO_PIN_OLED_SCK       33
#elif defined(USE_OLED_I2C)
#define GPIO_PIN_OLED_SDA       32
#define GPIO_PIN_OLED_SCK       33
#define GPIO_PIN_OLED_RST       U8X8_PIN_NONE
#elif defined(HAS_TFT_SCREEN)
#define  GPIO_PIN_TFT_MOSI      32
#define  GPIO_PIN_TFT_SCLK      33
#define  GPIO_PIN_TFT_RST       16
#define  GPIO_PIN_TFT_DC        22
#define  GPIO_PIN_TFT_CS        2
#define  GPIO_PIN_TFT_BL        25
#endif
#define GPIO_PIN_RCSIGNAL_RX    13
#define GPIO_PIN_RCSIGNAL_TX    13
#define GPIO_PIN_LED_WS2812     15
#define GPIO_PIN_FAN_EN         17

#ifdef HAS_FIVE_WAY_BUTTON
#define GPIO_PIN_FIVE_WAY_INPUT1     39
#define GPIO_PIN_FIVE_WAY_INPUT2     35
#define GPIO_PIN_FIVE_WAY_INPUT3     34
#endif
// Output Power
#define MinPower            PWR_10mW
#define MaxPower            PWR_250mW
#define POWER_OUTPUT_VALUES {-15,-11,-8,-5,-1}

#define Regulatory_Domain_ISM_2400 1
#define TARGET_DIY_2400_TX_ESP32_SX1280_E28
#define HARDWARE_VERSION  "DIY2400_E28"
