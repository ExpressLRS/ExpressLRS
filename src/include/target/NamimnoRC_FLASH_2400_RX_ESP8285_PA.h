#ifndef DEVICE_NAME
#define DEVICE_NAME "Namimno 2400RX"
#endif

// GPIO pin definitions
#define GPIO_PIN_NSS            15
#define GPIO_PIN_BUSY           5
#define GPIO_PIN_DIO1           4
#define GPIO_PIN_MOSI           13
#define GPIO_PIN_MISO           12
#define GPIO_PIN_SCK            14
#define GPIO_PIN_RST            2
#define GPIO_PIN_LED_RED        16
#define GPIO_PIN_TX_ENABLE      9   // Required by diversity RX
#if defined(USE_DIVERSITY)
// Low = Ant1, High = Ant2, pulled high by external resistor
#define GPIO_PIN_ANTENNA_SELECT 0
#endif

// Output Power
#define POWER_OUTPUT_FIXED          3

#define Regulatory_Domain_ISM_2400 1
