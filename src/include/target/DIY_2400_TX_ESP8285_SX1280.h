#ifndef DEVICE_NAME
#define DEVICE_NAME "DIY2400 ESP8266"
#endif

// ESP8266 can't do halfduplex inverted (no matter how hard CapnBry tries)
// These are limited to being DupleTX, or built with DEBUG_TX_FREERUN

// GPIO pin definitions
#define GPIO_PIN_NSS            15
#define GPIO_PIN_BUSY           5
#define GPIO_PIN_DIO1           4
#define GPIO_PIN_MOSI           13
#define GPIO_PIN_MISO           12
#define GPIO_PIN_SCK            14
#define GPIO_PIN_RST            2
#define GPIO_PIN_RCSIGNAL_RX    3 // UART0 - current code doesn't support changing them
#define GPIO_PIN_RCSIGNAL_TX    1 // UART0
//#define GPIO_PIN_RX_ENABLE      9  // available to use for PA enable, but will need to change POWER_OUTPUT_VALUES
//#define GPIO_PIN_TX_ENABLE      10
#define GPIO_PIN_LED_RED        16
#define GPIO_PIN_BUTTON         0

// Output Power
#define MinPower PWR_10mW
#define MaxPower PWR_25mW
#define POWER_OUTPUT_VALUES {10,13}
