#ifndef DEVICE_NAME
#define DEVICE_NAME "ELRS 900RX"
#endif

// GPIO pin definitions
#define GPIO_PIN_NSS 15
#define GPIO_PIN_DIO0 4
#define GPIO_PIN_DIO1 5
#define GPIO_PIN_MOSI 13
#define GPIO_PIN_MISO 12
#define GPIO_PIN_SCK 14
#define GPIO_PIN_RST 2
#define GPIO_PIN_RCSIGNAL_RX -1 //only uses default uart pins so leave as -1
#define GPIO_PIN_RCSIGNAL_TX -1
#define GPIO_PIN_LED 16
#define GPIO_PIN_BUTTON 0

#define POWER_OUTPUT_FIXED 15 //MAX power for 900 RXes