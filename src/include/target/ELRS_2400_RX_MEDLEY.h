#ifndef DEVICE_NAME
#define DEVICE_NAME "ELRS 2.4 Medley"
#endif

#define GPIO_PIN_BUSY           5
#define GPIO_PIN_BUSY_2         9
#define GPIO_PIN_DIO1           4
#define GPIO_PIN_DIO1_2         10
#define GPIO_PIN_NSS            15
#define GPIO_PIN_NSS_2          0
#define GPIO_PIN_MOSI           13
#define GPIO_PIN_MISO           12
#define GPIO_PIN_SCK            14
#define GPIO_PIN_RST            16

#define WS2812_IS_GRB
#define GPIO_PIN_LED_WS2812     2

#define POWER_OUTPUT_FIXED      13 //MAX power for 2400 RXes that doesn't have PA is 12.5dbm
