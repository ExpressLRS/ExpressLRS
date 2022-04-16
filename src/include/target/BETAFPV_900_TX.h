#ifndef DEVICE_NAME
#define DEVICE_NAME          "BETAFPV 900Nano\0"
#endif

// Any device features
#define USE_SX1276_RFO_HF

// GPIO pin definitions
#define GPIO_PIN_NSS            5
#define GPIO_PIN_DIO0           4
#define GPIO_PIN_DIO1           2
#define GPIO_PIN_MOSI           23
#define GPIO_PIN_MISO           19
#define GPIO_PIN_SCK            18
#define GPIO_PIN_RST            14
#define GPIO_PIN_RX_ENABLE      27
#define GPIO_PIN_TX_ENABLE      26
#define GPIO_PIN_RCSIGNAL_RX    13
#define GPIO_PIN_RCSIGNAL_TX    13 // so we don't have to solder the extra resistor, we switch rx/tx using gpio mux
#define GPIO_PIN_LED            -1
#define GPIO_PIN_LED_BLUE       17
#define GPIO_PIN_LED_GREEN      16
#define GPIO_PIN_BUTTON         25

// Output Power
#define MinPower                PWR_100mW
#define MaxPower                PWR_500mW
#define POWER_OUTPUT_VALUES     {0,3,8}
