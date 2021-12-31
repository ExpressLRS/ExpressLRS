#ifndef DEVICE_NAME
#define DEVICE_NAME "Vantac Lite 2G4"
#endif

// Any device features

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
#define GPIO_PIN_RCSIGNAL_RX    13
#define GPIO_PIN_RCSIGNAL_TX    13

#define GPIO_PIN_LED_WS2812     33

// Output Power
#define MinPower                PWR_10mW
#define MaxPower                PWR_500mW
#define POWER_OUTPUT_VALUES     {-15,-11,-8,-5,-1,2}

#define Regulatory_Domain_ISM_2400 1
