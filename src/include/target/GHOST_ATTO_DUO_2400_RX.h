#define DEVICE_NAME "GHOST ATTO"
// There is some special handling for this target
#define TARGET_RX_GHOST_ATTO_V1

// GPIO pin definitions
// First SX1280 Radio
#define GPIO_PIN_NSS            PA15
#define GPIO_PIN_BUSY           PA3
#define GPIO_PIN_DIO1           PA1
// Second SX1280 Radio
#define GPIO_PIN_NSS_2            PA11
#define GPIO_PIN_BUSY_2           PA4
#define GPIO_PIN_DIO1_2           PB7

#define GPIO_PIN_MOSI           PB5
#define GPIO_PIN_MISO           PB4
#define GPIO_PIN_SCK            PB3
#define GPIO_PIN_RST            PB0
#define GPIO_PIN_RCSIGNAL_RX    PB6 // USART1, half duplex
#define GPIO_PIN_RCSIGNAL_TX    PA2 // USART2, half duplex
#define GPIO_PIN_LED_WS2812      PA7
#define GPIO_PIN_LED_WS2812_FAST PA_7
// #define GPIO_PIN_BUTTON         PA12 // We do not use the button

// Output Power - use default SX1280

#define Regulatory_Domain_ISM_2400 1
