#define DEVICE_NAME "iFlight 2400TX"

// Any device features
#define USE_SX1280_DCDC

// GPIO pin definitions
#define GPIO_PIN_NSS            5
#define GPIO_PIN_BUSY           25
#define GPIO_PIN_DIO1           17
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
#define MinPower PWR_10mW
#define MaxPower PWR_250mW
#define POWER_OUTPUT_VALUES {-18,-15,-11,-8,-4}

#define Regulatory_Domain_ISM_2400 1
