#define DEVICE_NAME "DIY2400 E28 Man Kim FPV"


// Any device features
//#define USE_OLED_SPI
#define USE_OLED_I2C
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
// #define GPIO_PIN_OLED_CS        15
// #define GPIO_PIN_OLED_RST       16
// #define GPIO_PIN_OLED_DC        17
// #define GPIO_PIN_OLED_MOSI      32
// #define GPIO_PIN_OLED_SCK       33
#define GPIO_PIN_RCSIGNAL_RX    13
#define GPIO_PIN_RCSIGNAL_TX    13
#define GPIO_PIN_LED_WS2812     15
#define GPIO_PIN_FAN_EN         17
#define GPIO_PIN_OLED_SCK       22
#define GPIO_PIN_OLED_SDA       21
#define GPIO_PIN_OLED_RST       -1

// Output Power
#define MinPower            PWR_10mW
#define MaxPower            PWR_500mW
#define POWER_OUTPUT_VALUES {-15,-11,-8,-5,-1,3}


#define Regulatory_Domain_ISM_2400 1
