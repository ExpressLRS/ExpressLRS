// This target extends the BETAFPV_2400_TX target

#undef TX_DEVICE_NAME
#define TX_DEVICE_NAME              "BETAFPV 2400TX Micro"

// There is some special handling for this target
#undef TARGET_TX_BETAFPV_2400_V1
#define TARGET_TX_BETAFPV_2400_MICRO_V1


#undef GPIO_PIN_LED_BLUE      
#undef GPIO_PIN_LED_GREEN     
#define GPIO_PIN_FAN            17
#define GPIO_PIN_LED_WS2812     16
#define GPIO_PIN_OLED_RST -1
#define GPIO_PIN_OLED_SCK 32
#define GPIO_PIN_OLED_SDA 22

#define HAS_OLED
#define HAS_OLED_I2C
