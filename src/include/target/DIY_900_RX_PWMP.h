#ifndef DEVICE_NAME
#define DEVICE_NAME "DIY900 PWMP"
#endif

#define RADIO_SX127X
#define CRSF_RCVR_NO_SERIAL

// GPIO pin definitions
// same as DIY_900_RX_ESP8285_SX127x except with no serial/button and PWM outputs
#define GPIO_PIN_NSS            15
#define GPIO_PIN_DIO0           4
#define GPIO_PIN_MOSI           13
#define GPIO_PIN_MISO           12
#define GPIO_PIN_SCK            14
#define GPIO_PIN_RST            2
//#define GPIO_PIN_RCSIGNAL_RX -1 // does not use UART
//#define GPIO_PIN_RCSIGNAL_TX -1
#define GPIO_PIN_LED_RED        16
#if defined(DEBUG_LOG)
#define GPIO_PIN_PWM_OUTPUTS    {0, 5, 9, 10}
#else
#define GPIO_PIN_PWM_OUTPUTS    {0, 1, 3, 5, 9, 10}
#endif