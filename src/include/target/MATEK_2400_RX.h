#ifndef DEVICE_NAME
#define DEVICE_NAME "MATEK R24"
#endif

#define RADIO_SX1280

// GPIO pin definitions
#define GPIO_PIN_NSS                15
#define GPIO_PIN_BUSY               5
#define GPIO_PIN_DIO1               4
#define GPIO_PIN_MOSI               13
#define GPIO_PIN_MISO               12
#define GPIO_PIN_SCK                14
#define GPIO_PIN_RST                2
#define GPIO_PIN_LED                16
#define GPIO_PIN_BUTTON             0
#define GPIO_PIN_TX_ENABLE          10
#ifdef USE_DIVERSITY
    #define GPIO_PIN_ANTENNA_SELECT 9
#endif

// Output Power
#define POWER_OUTPUT_FIXED          3
