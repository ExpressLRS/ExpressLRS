#define DEVICE_NAME              "BETAFPV 2400TX Micro"

// Any device features
#define USE_OLED_I2C
#define OLED_REVERSED
#define HAS_FIVE_WAY_BUTTON

// There is some special handling for this target
#define TARGET_TX_BETAFPV_2400_MICRO_V1

// GPIO pin definitions
#define GPIO_PIN_NSS            5
#define GPIO_PIN_BUSY           21
#define GPIO_PIN_DIO0           -1
#define GPIO_PIN_DIO1           4
#define GPIO_PIN_MOSI           23
#define GPIO_PIN_MISO           19
#define GPIO_PIN_SCK            18
#define GPIO_PIN_RST            14
#define GPIO_PIN_RX_ENABLE      27
#define GPIO_PIN_TX_ENABLE      26
#define GPIO_PIN_RCSIGNAL_RX    13
#define GPIO_PIN_RCSIGNAL_TX    13
#define GPIO_PIN_FAN_EN         17
#define GPIO_PIN_LED_WS2812     16
#define GPIO_PIN_OLED_RST       -1
#define GPIO_PIN_OLED_SCK       32
#define GPIO_PIN_OLED_SDA       22
#define GPIO_PIN_BUTTON         25

// Output Power
#define MinPower PWR_10mW
#define MaxPower PWR_500mW
#define POWER_OUTPUT_VALUES {-18,-15,-13,-9,-4,3}

/* Joystick values              {UP, DOWN, LEFT, RIGHT, ENTER, IDLE}*/
#define JOY_ADC_VALUES          {1850, 900, 490, 1427, 0, 2978}

#define Regulatory_Domain_ISM_2400 1
