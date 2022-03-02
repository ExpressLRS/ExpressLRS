#ifndef DEVICE_NAME
#define DEVICE_NAME "DIY2400 PWMP"
#endif

#define USE_ANALOG_VBAT
#define CRSF_RCVR_NO_SERIAL

// GPIO pin definitions
// same as TARGET_RX_ESP8266_SX1280 except with no serial/button and PWM outputs
#define GPIO_PIN_NSS            15
#define GPIO_PIN_BUSY           5
#define GPIO_PIN_DIO1           4
#define GPIO_PIN_MOSI           13
#define GPIO_PIN_MISO           12
#define GPIO_PIN_SCK            14
#define GPIO_PIN_RST            2
//#define GPIO_PIN_RCSIGNAL_RX -1 // does not use UART
//#define GPIO_PIN_RCSIGNAL_TX -1
#define GPIO_PIN_LED_RED        16 // LED_RED on RX
#if defined(DEBUG_LOG)
#define GPIO_PIN_PWM_OUTPUTS    {0, 9, 10}
#else
#define GPIO_PIN_PWM_OUTPUTS    {0, 1, 3, 9, 10}
#endif

#if !defined(ANALOG_VBAT_SCALE)
#define ANALOG_VBAT_SCALE       561
#endif

#define POWER_OUTPUT_FIXED 13 //MAX power for 2400 RXes that doesn't have PA is 12.5dbm
