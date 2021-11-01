#if !defined(TX_DEVICE_NAME)
    #define TX_DEVICE_NAME "DIY2400 E28"
#endif

// GPIO pin definitions
#define GPIO_PIN_NSS            5
#define GPIO_PIN_BUSY           21
#define GPIO_PIN_DIO1           4
#define GPIO_PIN_MOSI           23
#define GPIO_PIN_MISO           19
#define GPIO_PIN_SCK            18
#define GPIO_PIN_RST            14
#define GPIO_PIN_RX_ENABLE      27
#define GPIO_PIN_TX_ENABLE      26
#define GPIO_PIN_OLED_SDA       -1
#define GPIO_PIN_OLED_SCK       -1
#define GPIO_PIN_RCSIGNAL_RX    13
#define GPIO_PIN_RCSIGNAL_TX    13
#define GPIO_PIN_LED_WS2812     15
#define GPIO_PIN_FAN_EN         17
#define HAS_OLED

// Output Power
#if !defined(MinPower)
    #define MinPower            PWR_10mW
#endif
#if !defined(MaxPower)
    #define MaxPower            PWR_250mW
#endif
#if !defined(POWER_OUTPUT_VALUES)
    #define POWER_OUTPUT_VALUES {-15,-11,-8,-5,-1}
#endif
