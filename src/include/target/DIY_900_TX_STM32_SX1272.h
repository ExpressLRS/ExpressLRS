#ifndef DEVICE_NAME
#define DEVICE_NAME "ELRS 900TX"
#endif

#define TARGET_DIY_900_TX_STM32_PCB

#define BACKPACK_LOGGING_BAUD 420000

// GPIO pin definitions
#define GPIO_PIN_NSS         PA4
#define GPIO_PIN_MOSI        PA7
#define GPIO_PIN_MISO        PA6
#define GPIO_PIN_SCK         PA5

#define GPIO_PIN_RST         PB0
#define GPIO_PIN_DIO0        PB1
#define GPIO_PIN_DIO1        PB2
#define GPIO_PIN_BUSY        PB3

#define GPIO_PIN_RX_ENABLE   PA2
#define GPIO_PIN_TX_ENABLE   PA3

#define GPIO_PIN_RCSIGNAL_RX PB11  // UART 3
#define GPIO_PIN_RCSIGNAL_TX PB10  // UART 3

#define GPIO_PIN_DEBUG_RX    PB7  // UART 1
#define GPIO_PIN_DEBUG_TX    PB6  // UART 1

#define GPIO_PIN_LED_GREEN   PA10
#define GPIO_LED_GREEN_INVERTED 0

#define GPIO_PIN_LED GPIO_PIN_LED_GREEN

#define RADIO_SX1272

// Output Power
#define MinPower                PWR_10mW
#define MaxPower                PWR_50mW
#define DefaultPower            PWR_50mW
#define POWER_OUTPUT_VALUES     {120,124,127}
