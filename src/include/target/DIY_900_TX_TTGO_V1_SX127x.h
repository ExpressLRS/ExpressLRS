// The name of the device in the LUA module
#define DEVICE_NAME "DIY900 TTGO V1"

// Any device features
#define USE_OLED_I2C

// GPIO pin definitions
#define GPIO_PIN_NSS 18
#define GPIO_PIN_DIO0 26
#define GPIO_PIN_MOSI 27
#define GPIO_PIN_MISO 19
#define GPIO_PIN_SCK 5
#define GPIO_PIN_RST 14
#define GPIO_PIN_OLED_SDA 4
#define GPIO_PIN_OLED_SCK 15
#define GPIO_PIN_OLED_RST 16
#define GPIO_PIN_RCSIGNAL_RX 13
#define GPIO_PIN_RCSIGNAL_TX 13
#define GPIO_PIN_LED 2
#define GPIO_PIN_BUTTON 0

// Output Power
#define MinPower PWR_10mW
#define MaxPower PWR_50mW
#define POWER_OUTPUT_VALUES {8,12,15}
