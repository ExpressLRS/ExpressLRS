#ifndef DEVICE_NAME
#define DEVICE_NAME "ELRS 900RX"
#endif

#define TARGET_DIY_900_RX_STM32

//#define USE_SX1276_RFO_HF false // disable PA Boost.

//Low - Mods on FHSS.CPP and user_define.txt
#define GPIO_PIN_RX_ENABLE PB2 // Low band.
//High  - Mods on FHSS.CPP and user_define.txt
#define GPIO_PIN_TX_ENABLE PB3 

// GPIO pin definitions
#define GPIO_PIN_NSS         PA4
#define GPIO_PIN_MOSI        PA7
#define GPIO_PIN_MISO        PA6
#define GPIO_PIN_SCK         PA5

#define GPIO_PIN_DIO0        PA0
#define GPIO_PIN_DIO1        PA1
#define GPIO_PIN_RST         PA2
#define GPIO_PIN_BUSY        PA3

#define GPIO_PIN_RCSIGNAL_RX PA10 // USART1
#define GPIO_PIN_RCSIGNAL_TX PA9  // USART1

#define GPIO_PIN_DEBUG_RX    PB11 // USART3
#define GPIO_PIN_DEBUG_TX    PB10 // USART3

#define GPIO_PIN_LED_GREEN   PC13
#define GPIO_LED_GREEN_INVERTED 0

// Output Power
#define MinPower                PWR_10mW
#define MaxPower                PWR_50mW
#define DefaultPower            PWR_50mW
#define POWER_OUTPUT_VALUES     {120,124,127}
