// The name of the device in the LUA module
#define DEVICE_NAME "DIY900 E19"

// GPIO pin definitions
#define GPIO_PIN_NSS 5
#define GPIO_PIN_DIO0 26
#define GPIO_PIN_DIO1 25
#define GPIO_PIN_MOSI 23
#define GPIO_PIN_MISO 19
#define GPIO_PIN_SCK 18
#define GPIO_PIN_RST 14
#define GPIO_PIN_RX_ENABLE 13
#define GPIO_PIN_TX_ENABLE 12
#define GPIO_PIN_RCSIGNAL_RX 2
#define GPIO_PIN_RCSIGNAL_TX 2 // so we don't have to solder the extra resistor, we switch rx/tx using gpio mux
#define GPIO_PIN_LED 27

// Output Power
#define MinPower    PWR_50mW
#define HighPower   PWR_250mW
#define MaxPower    PWR_1000mW
#define POWER_OUTPUT_VALUES {2,5,8,12,15}
