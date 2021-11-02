// This target extends the BETAFPV_900_TX target

#undef TX_DEVICE_NAME
#define TX_DEVICE_NAME              "BETAFPV 900TX Micro"

// There is some special handling for this target
#undef TARGET_TX_BETAFPV_900_V1
#define TARGET_TX_BETAFPV_900_MICRO_V1

// Any device features
#define HAS_OLED
#define HAS_OLED_I2C
#define OLED_REVERSED

// GPIO pin definitions
#undef GPIO_PIN_LED            
#undef GPIO_PIN_LED_BLUE       
#undef GPIO_PIN_LED_GREEN      
#define GPIO_PIN_FAN            17
#define GPIO_PIN_LED            16
#define GPIO_PIN_OLED_RST       -1
#define GPIO_PIN_OLED_SCK       32
#define GPIO_PIN_OLED_SDA       22


