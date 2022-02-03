#ifndef DEVICE_NAME
#define DEVICE_NAME "ELRS 2400RX"
#endif

// GPIO pin definitions
#define GPIO_PIN_NSS            15
#define GPIO_PIN_BUSY           5
#define GPIO_PIN_DIO1           4
#define GPIO_PIN_MOSI           13
#define GPIO_PIN_MISO           12
#define GPIO_PIN_SCK            14
#define GPIO_PIN_RST            2
#define GPIO_PIN_LED_RED        16 // LED_RED on TX, copied to LED on RX
#if defined(USE_DIVERSITY)
#define GPIO_PIN_ANTENNA_SELECT 0 // Low = Ant1, High = Ant2, pulled high by external resistor
#endif

// Output Power - use default SX1280

#define Regulatory_Domain_ISM_2400 1

#define MinPower            PWR_10mW
#define MaxPower            PWR_100mW
#define DefaultPower        PWR_100mW
#define POWER_OUTPUT_VALUES {-10,-6,0,10}
