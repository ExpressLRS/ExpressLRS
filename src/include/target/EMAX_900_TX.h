#ifndef DEVICE_NAME
    #ifdef EMAX_900_HAS_OLED
        #define DEVICE_NAME          "EMAX 900TX"
    #else
        #define DEVICE_NAME          "EMAX NANO 900TX"
    #endif
#endif

// GPIO pin definitions
#define GPIO_PIN_NSS            5
#define GPIO_PIN_DIO0           4
#define GPIO_PIN_MOSI           23
#define GPIO_PIN_MISO           19
#define GPIO_PIN_SCK            18
#define GPIO_PIN_RST            14
#define GPIO_PIN_RX_ENABLE      12
#define GPIO_PIN_RCSIGNAL_RX    13
#define GPIO_PIN_RCSIGNAL_TX    13 
#define GPIO_PIN_FAN_EN         34
#define GPIO_PIN_RFamp_APC2     26

#define USE_TX_BACKPACK
#define GPIO_PIN_DEBUG_RX       16
#define GPIO_PIN_DEBUG_TX       17
#define GPIO_PIN_BACKPACK_EN    25
#define GPIO_PIN_BACKPACK_BOOT  15

#ifdef EMAX_900_HAS_OLED
    #define USE_OLED_I2C
    #define OLED_REVERSED
    #define HAS_FIVE_WAY_BUTTON

    #define GPIO_PIN_OLED_SDA       22
    #define GPIO_PIN_OLED_SCK       21
    #define GPIO_PIN_OLED_RST       U8X8_PIN_NONE
    
    #define GPIO_PIN_JOYSTICK       33
    /* Joystick values              {UP, DOWN, LEFT, RIGHT, ENTER, IDLE}*/
    #define JOY_ADC_VALUES          {1650, 1070, 580, 2170, 0, 3240}
#else
    #define GPIO_PIN_LED_WS2812     27
    #define WS2812_IS_GRB
#endif

// Output Power
#define MinPower                PWR_10mW
#define MaxPower                PWR_2000mW
#define POWER_OUTPUT_DACWRITE
#define POWER_OUTPUT_VALUES     {0,5,15,25,45,105,150,225}
