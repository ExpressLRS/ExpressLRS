#ifndef DEVICE_NAME
#define DEVICE_NAME              "BETAFPV 900Micro"
#endif

// Any device features
#define USE_SX1276_RFO_HF
#define USE_OLED_I2C
#define OLED_REVERSED
#define HAS_FIVE_WAY_BUTTON
#define WS2812_IS_GRB

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
#define GPIO_PIN_FAN_EN         17
#define GPIO_PIN_LED_WS2812     16
#define GPIO_PIN_OLED_RST       -1
#define GPIO_PIN_OLED_SCK       32
#define GPIO_PIN_OLED_SDA       22
// #define GPIO_PIN_BUTTON         25
#define GPIO_PIN_JOYSTICK       25

// Output Power
#define MinPower                PWR_100mW
#define MaxPower                PWR_500mW
#define POWER_OUTPUT_VALUES     {0,3,8}

/* Joystick values              {UP, DOWN, LEFT, RIGHT, ENTER, IDLE}*/
#define JOY_ADC_VALUES          {2839, 2191, 1616, 3511, 0, 4095}
