#define DEVICE_NAME "AXIS THOR 2400TX"

#define USE_TX_BACKPACK

#define HAS_TFT_SCREEN

#define HAS_GSENSOR
#define HAS_GSENSOR_STK8xxx

#define HAS_THERMAL
#define HAS_THERMAL_LM75A

#define TX_UART_BAUD 420000

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

#define GPIO_PIN_INPUT_KEY1     39
#define GPIO_PIN_INPUT_KEY2     35
#define GPIO_PIN_INPUT_KEY3     34

#define GPIO_PIN_TFT_MOSI       16 
#define GPIO_PIN_TFT_SCLK       17  
#define GPIO_PIN_TFT_RST        0 
#define GPIO_PIN_TFT_DC         12
#define GPIO_PIN_TFT_CS         22 
#define GPIO_PIN_TFT_BL         15
#define GPIO_PIN_SCL            32
#define GPIO_PIN_SDA            33
#define GPIO_PIN_GSENSOR_INT    37
#define GPIO_PIN_LED_WS2812     25

#define STRING_WEB_UPDATE_TX_HOST   "elrs_tx"
#define STRING_WEB_UPDATE_TX_SSID   "ExpressLRS TX"
#define STRING_WEB_UPDATE_PWD   "expresslrs"
#define STRING_WEB_UPDATE_IP    "10.0.0.1"
#define STRING_THOR_VERSION     "2.0"

// Output Power
#if !defined(MinPower)
    #define MinPower            PWR_10mW
#endif
#if !defined(MaxPower)
    #define MaxPower            PWR_1000mW
#endif
#if !defined(POWER_OUTPUT_VALUES)
    #define POWER_OUTPUT_VALUES {-16,-12,-9,-6,-2,0,7}
#endif
