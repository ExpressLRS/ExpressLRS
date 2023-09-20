#ifndef DEVICE_NAME
#define DEVICE_NAME "Namimno 900RX"
#endif

// GPIO pin definitions
#define GPIO_PIN_RST            PC14
#define GPIO_PIN_DIO0           PA15
#define GPIO_PIN_DIO1           PA1
#define GPIO_PIN_NSS            PB12
#define GPIO_PIN_MOSI           PB15
#define GPIO_PIN_MISO           PB14
#define GPIO_PIN_SCK            PB13
#define GPIO_PIN_LED_RED        PA11
// RF Switch: LOW = RX, HIGH = TX
#define GPIO_PIN_TX_ENABLE      PB3

#define GPIO_PIN_RCSIGNAL_RX    PA10
#define GPIO_PIN_RCSIGNAL_TX    PA9

// Output Power - default for SX127x
#define POWER_OUTPUT_FIXED 127 //MAX power for 900 RXes
