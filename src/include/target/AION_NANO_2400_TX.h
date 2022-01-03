#ifndef DEVICE_NAME
#define DEVICE_NAME "AION NANO"
#endif

// Any device features
#define USE_OLED_I2C
#define USE_SX1280_DCDC
#define HAS_FIVE_WAY_BUTTON

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
#define GPIO_PIN_OLED_SDA       32
#define GPIO_PIN_OLED_SCK       33
#define GPIO_PIN_OLED_RST       U8X8_PIN_NONE
#define GPIO_PIN_RCSIGNAL_RX    13
#define GPIO_PIN_RCSIGNAL_TX    13
#define GPIO_PIN_LED_WS2812     15
#define GPIO_PIN_FAN_EN         17
#define GPIO_PIN_JOYSTICK       35

// Output Power
#define MinPower PWR_10mW
#define MaxPower PWR_500mW
#define POWER_OUTPUT_VALUES {-18,-15,-13,-9,-4,3}

#ifndef JOY_ADC_VALUES
/* Joystick values              {UP, DOWN, LEFT, RIGHT, ENTER, IDLE}*/
#define JOY_ADC_VALUES          {870, 600, 230, 35, 0, 4095}
#endif

#define Regulatory_Domain_ISM_2400 1