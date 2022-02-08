#define DEVICE_NAME "iFlight 900TX"

// Any device features
#define TARGET_TX_IFLIGHT
#define RADIO_SX127X
#define USE_SX1276_RFO_HF

// GPIO pin definitions
#define GPIO_PIN_NSS            5
#define GPIO_PIN_DIO0           17
#define GPIO_PIN_DIO1           16
#define GPIO_PIN_MOSI           23
#define GPIO_PIN_MISO           19
#define GPIO_PIN_SCK            18
#define GPIO_PIN_RST            22
#define GPIO_PIN_RX_ENABLE      21
#define GPIO_PIN_TX_ENABLE      4
#define GPIO_PIN_RCSIGNAL_RX    13
#define GPIO_PIN_RCSIGNAL_TX    13
#define GPIO_PIN_LED_RED        14
#define GPIO_PIN_LED_GREEN      27
#define GPIO_PIN_LED_BLUE       26
#define GPIO_PIN_BUTTON         15

// Output Power
#define MinPower                PWR_100mW
#define MaxPower                PWR_1000mW
#define POWER_OUTPUT_VALUES     {0,4,7,11}
