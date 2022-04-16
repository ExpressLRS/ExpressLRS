#ifndef DEVICE_NAME
#define DEVICE_NAME "DIY2400 PWMP\0\0\0\0"
#endif

#define USE_ANALOG_VBAT
#define CRSF_RCVR_NO_SERIAL

#if !defined(PWMP_CH_CNT)
// 5 output PWMP: connects BUSY and RST to SX1280
// 6 output PWMP: connects just RST, BUSY used as PWM output
// 7 output PWMP: BUSY and RST used as PWM output, not connected to SX1280
#define PWMP_CH_CNT 5
#endif

// GPIO pin definitions
// same as TARGET_RX_ESP8266_SX1280 except:
// no serial/button, possibly reuse the BUSY/RST pin and adds PWM outputs
#define GPIO_PIN_NSS            15
#define GPIO_PIN_DIO1           4
#define GPIO_PIN_MOSI           13
#define GPIO_PIN_MISO           12
#define GPIO_PIN_SCK            14
#define GPIO_PIN_LED_RED        16 // LED_RED on RX
//#define GPIO_PIN_RCSIGNAL_RX -1 // does not use UART
//#define GPIO_PIN_RCSIGNAL_TX -1
#if PWMP_CH_CNT < 6
#define GPIO_PIN_BUSY           5
#define PWMP_OUTPUTS_BUSY
#else
#define PWMP_OUTPUTS_BUSY       5,
#endif
#if PWMP_CH_CNT < 7
#define GPIO_PIN_RST            2
#define PWMP_OUTPUTS_RST
#else
#define PWMP_OUTPUTS_RST        2,
#endif

#if defined(DEBUG_LOG)
#define PWMP_OUTPUTS_DEBUGLOG
#else
#define PWMP_OUTPUTS_DEBUGLOG 1, 3,
#endif

// A full set would be { 0, 1, 3, 9, 10, 5, 2 }
#define GPIO_PIN_PWM_OUTPUTS { 0, PWMP_OUTPUTS_DEBUGLOG 9, 10, PWMP_OUTPUTS_BUSY PWMP_OUTPUTS_RST }

// Vbat = (adc - ANALOG_VBAT_OFFSET) / ANALOG_VBAT_SCALE
// OFFSET is needed becauae ESPs don't go down to 0 even if their ADC is grounded
#if !defined(ANALOG_VBAT_OFFSET)
#define ANALOG_VBAT_OFFSET      12
#endif
#if !defined(ANALOG_VBAT_SCALE)
#define ANALOG_VBAT_SCALE       410
#endif

#define POWER_OUTPUT_FIXED 13 //MAX power for 2400 RXes that doesn't have PA is 12.5dbm
