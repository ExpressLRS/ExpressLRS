#ifndef DEVICE_NAME
    #if EMAX_2400_HAS_OLED
        #define DEVICE_NAME          "EMAX 2400TX"
    #else
        #define DEVICE_NAME          "EMAX NANO 2400TX"
    #endif
#endif

#define USE_TX_BACKPACK
#define GPIO_PIN_DEBUG_RX       16
#define GPIO_PIN_DEBUG_TX       17
#define GPIO_PIN_BACKPACK_EN    25
#define GPIO_PIN_BACKPACK_BOOT  15

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
#define GPIO_PIN_FAN_EN         22

#ifdef EMAX_2400_HAS_OLED
    #define USE_OLED_I2C
    #define OLED_REVERSED
    #define HAS_FIVE_WAY_BUTTON

    #define GPIO_PIN_OLED_SDA       35
    #define GPIO_PIN_OLED_SCK       34
    #define GPIO_PIN_OLED_RST       U8X8_PIN_NONE
    
    #define GPIO_PIN_JOYSTICK       33  

    /* Joystick values              {UP, DOWN, LEFT, RIGHT, ENTER, IDLE}*/
    #define JOY_ADC_VALUES          {1905, 1160, 580, 2580, 0, 4095}
#else
    #define TARGET_TX_EMAX_NANO_2400_V1

    #define GPIO_PIN_LED_BLUE       34
    #define GPIO_PIN_LED_GREEN      35
#endif

// Output Power
#define MinPower PWR_10mW
#define MaxPower PWR_1000mW
#define POWER_OUTPUT_VALUES {-19,-15,-12,-9,-4,-1,3}

#define Regulatory_Domain_ISM_2400 1
