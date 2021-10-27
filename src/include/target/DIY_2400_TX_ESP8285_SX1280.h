#define TX_DEVICE_NAME "DIY2400 ESP8266"

// GPIO pin definitions
#define GPIO_PIN_NSS            15
#define GPIO_PIN_BUSY           5
#define GPIO_PIN_DIO1           4
#define GPIO_PIN_MOSI           13
#define GPIO_PIN_MISO           12
#define GPIO_PIN_SCK            14
#define GPIO_PIN_RST            2
//#define GPIO_PIN_RCSIGNAL_RX -1 // uses default uart pins
//#define GPIO_PIN_RCSIGNAL_TX -1
#define GPIO_PIN_LED_RED        16 // LED_RED on TX, copied to LED on RX
#define GPIO_PIN_BUTTON         0

// Output Power
#define MinPower PWR_10mW
#define MaxPower PWR_10mW
#define POWER_OUTPUT_VALUES {13}
