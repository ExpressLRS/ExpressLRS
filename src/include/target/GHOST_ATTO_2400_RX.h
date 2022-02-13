#ifndef DEVICE_NAME
#define DEVICE_NAME "GHOST ATTO"
#endif
// There is some special handling for this target
#define TARGET_RX_GHOST_ATTO_V1

// GPIO pin definitions
#define GPIO_PIN_NSS            PA15
#define GPIO_PIN_BUSY           PA3
#define GPIO_PIN_DIO1           PA1
#define GPIO_PIN_MOSI           PB5
#define GPIO_PIN_MISO           PB4
#define GPIO_PIN_SCK            PB3
#define GPIO_PIN_RST            PB0
//#define GPIO_PIN_RCSIGNAL_RX    PB7
//#define GPIO_PIN_RCSIGNAL_TX    PB6
#define GPIO_PIN_RCSIGNAL_RX    PB6 // USART1, half duplex
#define GPIO_PIN_RCSIGNAL_TX    PA2 // USART2, half duplex
//#define GPIO_PIN_LED            PA7
#define GPIO_PIN_LED_WS2812      PA7
#define GPIO_PIN_LED_WS2812_FAST PA_7
//#define GPIO_PIN_BUTTON         PA12

// Output Power - use default SX1280
