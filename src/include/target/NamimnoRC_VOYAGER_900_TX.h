#define TX_DEVICE_NAME "Namimno Voyager"

#define USE_TX_BACKPACK

// GPIO pin definitions
#define GPIO_PIN_NSS                PB12
#define GPIO_PIN_DIO0               PA15
#define GPIO_PIN_MOSI               PB15
#define GPIO_PIN_MISO               PB14
#define GPIO_PIN_SCK                PB13
#define GPIO_PIN_RST                PC14
#define GPIO_PIN_RX_ENABLE          PB3  //HIGH = RX, LOW = TX
/* DAC settings */
#define GPIO_PIN_SDA                PB9
#define GPIO_PIN_SCL                PB8

/* S.Port input signal */
#define GPIO_PIN_RCSIGNAL_RX        PB11 /* USART3 */
#define GPIO_PIN_RCSIGNAL_TX        PB10 /* USART3 */
#define GPIO_PIN_BUFFER_OE          PA1
#define GPIO_PIN_BUFFER_OE_INVERTED 1
#define GPIO_PIN_FAN_EN             PB1
/* Backpack logger connection */
#ifdef USE_ESP8266_BACKPACK
    #define GPIO_PIN_DEBUG_RX       PA10
    #define GPIO_PIN_DEBUG_TX       PA9
#else
    #define GPIO_PIN_DEBUG_RX       PA3
    #define GPIO_PIN_DEBUG_TX       PA2
#endif
/* WS2812 led */
#define GPIO_PIN_LED_WS2812         PB0
#define GPIO_PIN_LED_WS2812_FAST    PB_0

// Output Power
#define POWER_OUTPUT_DAC            0b0001101
#define MinPower                    PWR_10mW
#define MaxPower                    PWR_2000mW
#if defined(Regulatory_Domain_EU_868)
#define POWER_OUTPUT_VALUES         {500,860,1000,1170,1460,1730,2100,2600}
#else
#define POWER_OUTPUT_VALUES         {895,1030,1128,1240,1465,1700,2050,2600}
#endif
