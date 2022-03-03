#ifndef DEVICE_NAME
#define DEVICE_NAME "DIY2400 E28"
#endif

// Any device features
#define USE_SX1280_DCDC
#if !defined(USE_OLED_I2C)
#define USE_OLED_SPI
#endif

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
#endif
#define GPIO_PIN_RCSIGNAL_RX    13
#define GPIO_PIN_RCSIGNAL_TX    13
#define GPIO_PIN_LED_WS2812     15
#define GPIO_PIN_FAN_EN         17

// Output Power
#define MinPower            PWR_10mW
#define MaxPower            PWR_250mW
#define POWER_OUTPUT_VALUES {-15,-11,-8,-5,-1}
