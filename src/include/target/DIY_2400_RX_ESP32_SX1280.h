#ifndef DEVICE_NAME
#define DEVICE_NAME "ESP32 RX 2400"
#endif

// GPIO pin definitions
#define GPIO_PIN_NSS            5
#define GPIO_PIN_BUSY           36
#define GPIO_PIN_DIO1           39
#define GPIO_PIN_MOSI           23
#define GPIO_PIN_MISO           19
#define GPIO_PIN_SCK            18
#define GPIO_PIN_RST            22



#define GPIO_PIN_LED_WS2812     15

// Output Power
#define POWER_OUTPUT_FIXED      13 // MAX power for 2400 RXes that don't have PA is 12.5dbm
