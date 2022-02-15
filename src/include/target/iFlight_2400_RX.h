#define DEVICE_NAME "iFlight 2400RX\0\0"

#define USE_SX1280_DCDC

// GPIO pin definitions
#define GPIO_PIN_NSS                15
#define GPIO_PIN_BUSY               4
#define GPIO_PIN_DIO1               5
#define GPIO_PIN_MOSI               13
#define GPIO_PIN_MISO               12
#define GPIO_PIN_SCK                14
#define GPIO_PIN_RST                2
#define GPIO_PIN_RX_ENABLE          9
#define GPIO_PIN_TX_ENABLE          10
#define GPIO_PIN_LED                16
#define GPIO_PIN_BUTTON             0

// Output Power

#define POWER_OUTPUT_FIXED 13 //MAX power for 2400 RXes that doesn't have PA is 12.5dbm
