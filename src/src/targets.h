#pragma once
#include <Arduino.h>

#define UNDEF_PIN (-1)

/// General Features ///
#define LED_MAX_BRIGHTNESS 50 //0..255 for max led brightness
/////////////////////////

#define WORD_ALIGNED_ATTR __attribute__((aligned(4)))

#ifdef PLATFORM_STM32
#define ICACHE_RAM_ATTR //nothing//
#else
#ifndef ICACHE_RAM_ATTR //fix to allow both esp32 and esp8266 to use ICACHE_RAM_ATTR for mapping to IRAM
#define ICACHE_RAM_ATTR IRAM_ATTR
#endif
#endif

#ifdef TARGET_TTGO_LORA_V1_AS_TX
#define GPIO_PIN_NSS 18
#define GPIO_PIN_BUSY           -1 // NOT USED ON THIS TARGET
#define GPIO_PIN_DIO0 26
#define GPIO_PIN_DIO1 -1
#define GPIO_PIN_MOSI 27
#define GPIO_PIN_MISO 19
#define GPIO_PIN_SCK 5
#define GPIO_PIN_RST 14
#define GPIO_PIN_OLED_SDA 4
#define GPIO_PIN_OLED_SCK 15
#define GPIO_PIN_RCSIGNAL_RX 13
#define GPIO_PIN_RCSIGNAL_TX 13
#define GPIO_PIN_LED 2
#endif

#ifdef TARGET_TTGO_LORA_V1_AS_RX
#endif

#ifdef TARGET_TTGO_LORA_V2_AS_TX
#define GPIO_PIN_NSS 18
#define GPIO_PIN_BUSY           -1 // NOT USED ON THIS TARGET
#define GPIO_PIN_DIO0 26
#define GPIO_PIN_DIO1 -1
#define GPIO_PIN_MOSI 27
#define GPIO_PIN_MISO 19
#define GPIO_PIN_SCK 5
#define GPIO_PIN_RST 14
#define GPIO_PIN_OLED_SDA 21
#define GPIO_PIN_OLED_SCK 22
#define GPIO_PIN_RCSIGNAL_RX 13
#define GPIO_PIN_RCSIGNAL_TX 13
#endif

#ifdef TARGET_TTGO_LORA_V2_AS_RX
 // not supported
#endif

#ifdef TARGET_EXPRESSLRS_PCB_TX_V3
#define GPIO_PIN_NSS 5
#define GPIO_PIN_BUSY           -1 // NOT USED ON THIS TARGET
#define GPIO_PIN_DIO0 26
#define GPIO_PIN_DIO1 25
#define GPIO_PIN_MOSI 23
#define GPIO_PIN_MISO 19
#define GPIO_PIN_SCK 18
#define GPIO_PIN_RST 14
#define GPIO_PIN_RX_ENABLE 13
#define GPIO_PIN_TX_ENABLE 12
#define GPIO_PIN_RCSIGNAL_RX 2
#define GPIO_PIN_RCSIGNAL_TX 2 // so we don't have to solder the extra resistor, we switch rx/tx using gpio mux
#define GPIO_PIN_LED 27
#endif

#ifdef TARGET_EXPRESSLRS_PCB_TX_V3_LEGACY
#define GPIO_PIN_BUTTON 36
#define RC_SIGNAL_PULLDOWN 4
#endif

#ifdef TARGET_EXPRESSLRS_PCB_RX_V3
#define GPIO_PIN_NSS 15
#define GPIO_PIN_BUSY           -1 // NOT USED ON THIS TARGET
#define GPIO_PIN_DIO0 4
#define GPIO_PIN_DIO1 5
#define GPIO_PIN_MOSI 13
#define GPIO_PIN_MISO 12
#define GPIO_PIN_SCK 14
#define GPIO_PIN_RST 2
#define GPIO_PIN_RCSIGNAL_RX -1 //only uses default uart pins so leave as -1
#define GPIO_PIN_RCSIGNAL_TX -1
#define GPIO_PIN_LED 16
#define GPIO_PIN_LED 16
#define GPIO_PIN_BUTTON 0
#define timerOffset -1
#endif

#ifdef TARGET_R9M_RX
/*
Credit to Jacob Walser (jaxxzer) for the pinout!!!
https://github.com/jaxxzer
*/
#define GPIO_PIN_NSS            PB12 //confirmed on SLIMPLUS, R900MINI
#define GPIO_PIN_BUSY           -1   // NOT USED ON THIS TARGET
#define GPIO_PIN_DIO0           PA15 //confirmed on SLIMPLUS, R900MINI
#define GPIO_PIN_DIO1           PA1  // NOT CORRECT!!! PIN STILL NEEDS TO BE FOUND BUT IS CURRENTLY UNUSED
/////////////////////////////////////// NOT FOUND ON SLIMPLUS EITHER.
#define GPIO_PIN_MOSI           PB15 //confirmed on SLIMPLUS, R900MINI
#define GPIO_PIN_MISO           PB14 //confirmed on SLIMPLUS, R900MINI
#define GPIO_PIN_SCK            PB13 //confirmed on SLIMPLUS, R900MINI
#define GPIO_PIN_RST            PC14 //confirmed on SLIMPLUS, R900MINI
#define GPIO_PIN_SDA            PB7
#define GPIO_PIN_SCL            PB6

#ifdef USE_R9MM_R9MINI_SBUS
    #define GPIO_PIN_RCSIGNAL_RX    PA3
    #define GPIO_PIN_RCSIGNAL_TX    PA2
#elif TARGET_R9SLIMPLUS_RX               // R9SLIMPLUS USES DUAL UART CONFIGURATION FOR TX1/RX1
    #define GPIO_PIN_RCSIGNAL_RX    PB11 // RX1 PIN OF CONNECTOR 1 ON SLIMPLUS
    #define GPIO_PIN_RCSIGNAL_TX    PA9  // TX1 PIN OF CONNECTOR 1 ON SLIMPLUS
#elif TARGET_R900MINI_RX
    #define GPIO_PIN_RCSIGNAL_RX    PA3 // convinient pin for direct chip solder
    #define GPIO_PIN_RCSIGNAL_TX    PA2 // convinient pin for direct chip solder
#else //default R9MM_R9MINI or R9MX
    #define GPIO_PIN_RCSIGNAL_RX    PA10
    #define GPIO_PIN_RCSIGNAL_TX    PA9
#endif

#ifdef TARGET_R9MX_RX
    //#define GPIO_PIN_LED            PB2 // Red
    #define GPIO_PIN_LED_RED        PB2 // Red
    #define GPIO_PIN_LED_GREEN      PB3 // Green
    #define GPIO_PIN_BUTTON         PB0  // pullup e.g. LOW when pressed
#elif TARGET_R9SLIMPLUS_RX
    //#define GPIO_PIN_LED            PA11 // Red
    #define GPIO_PIN_LED_RED        PA11 // Red
    #define GPIO_PIN_LED_GREEN      PA12 // Green
    #define GPIO_PIN_BUTTON         PC13  // pullup e.g. LOW when pressed
    /* PB3: RX = HIGH, TX = LOW */
    #define GPIO_PIN_RX_ENABLE      PB3
    /* PB9: antenna 1 (left) = HIGH, antenna 2 (right) = LOW
     * Note: Right Antenna is selected by default, LOW */
    #define GPIO_PIN_ANTENNA_SELECT PB9
#elif TARGET_R900MINI_RX
    #define GPIO_PIN_LED            PA11 // Red
    #define GPIO_PIN_LED_RED        PA11 // Red
    #define GPIO_PIN_LED_GREEN      PA12 // Green
    #define GPIO_PIN_BUTTON         PC13  // pullup e.g. LOW when pressed
#else //R9MM_R9MINI
    //#define GPIO_PIN_LED            PC1 // Red
    #define GPIO_PIN_LED_RED        PC1 // Red
    #define GPIO_PIN_LED_GREEN      PB3 // Green
    #define GPIO_PIN_BUTTON         PC13  // pullup e.g. LOW when pressed
#endif
#define timerOffset             1

// External pads
// #define R9m_Ch1    PA8
// #define R9m_Ch2    PA11
// #define R9m_Ch3    PA9
// #define R9m_Ch4    PA10
// #define R9m_sbus   PA2
// #define R9m_sport  PA5
// #define R9m_isport PB11

//method to set HSE and clock speed correctly//
// #if defined(HSE_VALUE)
// /* Redefine the HSE value; it's equal to 8 MHz on the STM32F4-DISCOVERY Kit */
//#undef HSE_VALUE
//#define HSE_VALUE ((uint32_t)16000000).
//#define HSE_VALUE    25000000U
// #endif /* HSE_VALUE */
//#define SYSCLK_FREQ_72MHz
#endif

#if defined(TARGET_R9M_TX) || defined(TARGET_TX_ES915TX)

#define GPIO_PIN_RFamp_APC1           PA6  //APC2 is connected through a I2C dac and is handled elsewhere
#define GPIO_PIN_RFswitch_CONTROL     PB3  //HIGH = RX, LOW = TX

#define GPIO_PIN_NSS            PB12
#define GPIO_PIN_BUSY           -1 // NOT USED ON THIS TARGET
#define GPIO_PIN_DIO0           PA15
#define GPIO_PIN_MOSI           PB15
#define GPIO_PIN_MISO           PB14
#define GPIO_PIN_SCK            PB13
#define GPIO_PIN_RST            PC14
#define GPIO_PIN_RX_ENABLE      GPIO_PIN_RFswitch_CONTROL
#define GPIO_PIN_TX_ENABLE      GPIO_PIN_RFamp_APC1
#define GPIO_PIN_SDA            PB7
#define GPIO_PIN_SCL            PB6
#define GPIO_PIN_RCSIGNAL_RX    PB11 // not yet confirmed
#define GPIO_PIN_RCSIGNAL_TX    PB10 // not yet confirmed
#define GPIO_PIN_LED_RED        PA11 // Red LED
#define GPIO_PIN_LED_GREEN      PA12 // Green LED
#define GPIO_PIN_BUTTON         PA8 // pullup e.g. LOW when pressed
#define GPIO_PIN_BUZZER         PB1
#define GPIO_PIN_DIP1           PA12 // dip switch 1
#define GPIO_PIN_DIP2           PA11 // dip switch 2
#define GPIO_PIN_FAN_EN         PB9 // Fan mod https://github.com/AlessandroAU/ExpressLRS/wiki/R9M-Fan-Mod-Cover

#define GPIO_PIN_DEBUG_RX    PA10 // confirmed
#define GPIO_PIN_DEBUG_TX    PA9 // confirmed


#define BUFFER_OE               PA5  //CONFIRMED
#define GPIO_PIN_DIO1           PA1  //Not Needed, HEARTBEAT pin
#endif

#ifdef TARGET_R9M_LITE_TX

#define GPIO_PIN_RFswitch_CONTROL     PC13  // need to confirm  //HIGH = RX, LOW = TX

#define GPIO_PIN_NSS            PB12
#define GPIO_PIN_DIO0           PC15
#define GPIO_PIN_DIO1           -1    //unused for sx1280
#define GPIO_PIN_BUSY           -1    //unused for sx1280
#define GPIO_PIN_MOSI           PB15
#define GPIO_PIN_MISO           PB14
#define GPIO_PIN_SCK            PB13
#define GPIO_PIN_RST            PC14
#define GPIO_PIN_RX_ENABLE      PC13 //PB3 // need to confirm
#define GPIO_PIN_SDA            PB7
#define GPIO_PIN_SCL            PB6
#define GPIO_PIN_RCSIGNAL_RX    PB11 // not yet confirmed
#define GPIO_PIN_RCSIGNAL_TX    PB10 // not yet confirmed
#define GPIO_PIN_LED_RED        PA1 // Red LED // not yet confirmed
#define GPIO_PIN_LED_GREEN      PA4 // Green LED // not yet confirmed

#define GPIO_PIN_DEBUG_RX    PA3 // confirmed
#define GPIO_PIN_DEBUG_TX    PA2 // confirmed

#define BUFFER_OE               PA5  //CONFIRMED

#endif

#ifdef TARGET_R9M_LITE_PRO_TX
//#define GPIO_PIN_RFamp_APC1           PA4  //2.7V
//#define GPIO_PIN_RFamp_APC2           PA5  //100mW@590mV, 200mW@870mV, 500mW@1.093V, 1W@1.493V
#define GPIO_PIN_RFswitch_CONTROL     PA6  // confirmed  //HIGH = RX, LOW = TX

#define GPIO_PIN_NSS            PB12 // confirmed
#define GPIO_PIN_DIO0           PA8  // confirmed
#define GPIO_PIN_DIO1           UNDEF_PIN   // NOT USED ON THIS TARGET
#define GPIO_PIN_BUSY           UNDEF_PIN   // NOT USED ON THIS TARGET
#define GPIO_PIN_MOSI           PB15
#define GPIO_PIN_MISO           PB14
#define GPIO_PIN_SCK            PB13
#define GPIO_PIN_RST            PA9  // NRESET
#define GPIO_PIN_RX_ENABLE      PC13 // need to confirm
#define GPIO_PIN_SDA            PB7
#define GPIO_PIN_SCL            PB6
#define GPIO_PIN_RCSIGNAL_RX    PB11 // not yet confirmed
#define GPIO_PIN_RCSIGNAL_TX    PB10 // not yet confirmed
#define GPIO_PIN_LED_GREEN      PA15 // Green LED
#define GPIO_PIN_LED_RED        PB3  // Red LED
#define GPIO_PIN_LED_RED        PB4  // Blue LED

#define GPIO_PIN_DEBUG_RX    	PA3  // not yet confirmed
#define GPIO_PIN_DEBUG_TX    	PA2  // not yet confirmed

#define GPIO_PIN_VRF1			PA7  // 26SU Sample RF1
#define GPIO_PIN_VRF2			PB1  // 26SU Sample RF2
#define GPIO_PIN_SWR			PA0  // SWR? ADC1_IN1

#define BUFFER_OE               UNDEF_PIN  //CONFIRMED

#endif

#ifdef TARGET_RX_ESP8266_SX1280_V1
#define GPIO_PIN_NSS 15
#define GPIO_PIN_BUSY 5
#define GPIO_PIN_DIO0 -1 // does not exist on sx1280
#define GPIO_PIN_DIO1 4
#define GPIO_PIN_MOSI 13
#define GPIO_PIN_MISO 12
#define GPIO_PIN_SCK 14
#define GPIO_PIN_RST 2
#define GPIO_PIN_RCSIGNAL_RX -1 //only uses default uart pins so leave as -1
#define GPIO_PIN_RCSIGNAL_TX -1
#define GPIO_PIN_LED 16
#define GPIO_PIN_BUTTON 0
#define timerOffset -1
#endif

#ifdef TARGET_TX_ESP32_SX1280_V1
#define GPIO_PIN_NSS 5
#define GPIO_PIN_BUSY 21
#define GPIO_PIN_DIO0 -1 // does not exist on sx1280
#define GPIO_PIN_DIO1 4
#define GPIO_PIN_MOSI 23
#define GPIO_PIN_MISO 19
#define GPIO_PIN_SCK 18
#define GPIO_PIN_RST 14
#define GPIO_PIN_RCSIGNAL_RX 13
#define GPIO_PIN_RCSIGNAL_TX 13
#endif

#ifdef TARGET_RX_GHOST_ATTO_V1
#define GPIO_PIN_NSS            PA15
#define GPIO_PIN_BUSY           PA3
#define GPIO_PIN_DIO0           -1 // does not exist on sx1280
#define GPIO_PIN_DIO1           PA1
#define GPIO_PIN_MOSI           PB5
#define GPIO_PIN_MISO           PB4
#define GPIO_PIN_SCK            PB3
#define GPIO_PIN_RST            PB0
//#define GPIO_PIN_RCSIGNAL_RX    PB7
//#define GPIO_PIN_RCSIGNAL_TX    PB6
#define GPIO_PIN_RCSIGNAL_RX    PB6 // USART1, half duplex
#define GPIO_PIN_RCSIGNAL_TX    PA2 // USART2, half duplex
//#define GPIO_PIN_LED            PA7
#define GPIO_PIN_LED_WS2812      PA7
#define GPIO_PIN_LED_WS2812_FAST PA_7
//#define GPIO_PIN_BUTTON         PA12
#define timerOffset             1
#endif


#ifdef TARGET_RX_GHOST_ATTO_DUO
// Shared sx1280 F301 pins
#define GPIO_PIN_MOSI           PB5
#define GPIO_PIN_MISO           PB4
#define GPIO_PIN_SCK            PB3
#define GPIO_PIN_RST            PB0
#define GPIO_PIN_DIO0           -1 // does not exist on sx1280
//======================================
/**
 *  For now I am just manually setting the left or right 1280 CC pin to HIGH and naming it NSS_2
 * We have full control over the pins!!! it works right now with just one , left or right. 
 * 
 **/
// First (Left) sc1280
#define GPIO_PIN_NSS_2            PA15
// #define GPIO_PIN_BUSY           PA3
// #define GPIO_PIN_DIO1           PA1
//=====================================
// Second (Right) sc1280
#define GPIO_PIN_NSS            PA11
#define GPIO_PIN_BUSY           PA4
#define GPIO_PIN_DIO1           PB7


//#define GPIO_PIN_RCSIGNAL_RX    PB7
//#define GPIO_PIN_RCSIGNAL_TX    PB6
// #define GPIO_PIN_RCSIGNAL_RX    PA4 // USART1, half duplex
// #define GPIO_PIN_RCSIGNAL_TX    PA3 // USART2, half duplex
#define GPIO_PIN_RCSIGNAL_RX    PB6 // USART1, half duplex
#define GPIO_PIN_RCSIGNAL_TX    PA2 // USART2, half duplex //pin is really PA3
// LED Pins
#define GPIO_PIN_LED_WS2812      PA7  
#define GPIO_PIN_LED_WS2812_FAST PA_7 
#define timerOffset             1
#endif

#ifdef TARGET_TX_GHOST
#define GPIO_PIN_NSS             PA15
#define GPIO_PIN_BUSY            PB15
#define GPIO_PIN_DIO0           -1 // does not exist on sx1280
#define GPIO_PIN_DIO1            PB2
#define GPIO_PIN_MOSI            PA7
#define GPIO_PIN_MISO            PA6
#define GPIO_PIN_SCK             PA5
#define GPIO_PIN_RST             PB0
#define GPIO_PIN_RX_ENABLE       PA8  // These may be swapped
#define GPIO_PIN_TX_ENABLE       PB14 // These may be swapped
#define GPIO_PIN_RCSIGNAL_RX     PA10 // S.PORT (Only needs one wire )
#define GPIO_PIN_RCSIGNAL_TX     PB6  // Needed for CRSF libs but does nothing/not hooked up to JR module.
#define GPIO_PIN_LED_WS2812      PB6
#define GPIO_PIN_LED_WS2812_FAST PB_6
#define GPIO_PIN_RF_AMP_EN       PB11 // https://www.skyworksinc.com/-/media/SkyWorks/Documents/Products/2101-2200/SE2622L_202733C.pdf
#define GPIO_PIN_RF_AMP_DET      PA3
#define GPIO_PIN_ANT_CTRL_1      PA9
#define GPIO_PIN_ANT_CTRL_2      PB13
#define GPIO_PIN_BUZZER          PC13
#define timerOffset              1
#endif

#if defined(TARGET_TX_ESP32_E28_SX1280_V1) || defined(TARGET_TX_ESP32_LORA1280F27)
#define GPIO_PIN_NSS 5
#define GPIO_PIN_BUSY 21
#define GPIO_PIN_DIO0 -1
#define GPIO_PIN_DIO1 4
#define GPIO_PIN_MOSI 23
#define GPIO_PIN_MISO 19
#define GPIO_PIN_SCK 18
#define GPIO_PIN_RST 14
#define GPIO_PIN_RX_ENABLE 27
#define GPIO_PIN_TX_ENABLE 26
#define GPIO_PIN_OLED_SDA -1
#define GPIO_PIN_OLED_SCK -1
#define GPIO_PIN_RCSIGNAL_RX 13
#define GPIO_PIN_RCSIGNAL_TX 13
#endif

#if defined(TARGET_SX1280_RX_CCG_NANO_v05)
#define GPIO_PIN_NSS         PA4
#define GPIO_PIN_MOSI        PA7
#define GPIO_PIN_MISO        PA6
#define GPIO_PIN_SCK         PA5

#define GPIO_PIN_DIO0        -1
#define GPIO_PIN_DIO1        PA10
#define GPIO_PIN_RST         PB4
#define GPIO_PIN_BUSY        PA11

#define GPIO_PIN_RCSIGNAL_RX PB7  // USART1, AFAIO
#define GPIO_PIN_RCSIGNAL_TX PB6  // USART1, AFAIO

#define GPIO_PIN_LED_RED     PB5

#define timerOffset          1
#endif /* TARGET_SX1280_RX_CCG_NANO_v05 */

#ifdef GPIO_PIN_LED_WS2812
#ifndef GPIO_PIN_LED_WS2812_FAST
#error "WS2812 support requires _FAST pin!"
#endif
#else
#define GPIO_PIN_LED_WS2812         UNDEF_PIN
#define GPIO_PIN_LED_WS2812_FAST    UNDEF_PIN
#endif

/* Set red led to default */
#ifndef GPIO_PIN_LED
#ifdef GPIO_PIN_LED_RED
#define GPIO_PIN_LED GPIO_PIN_LED_RED
#endif /* GPIO_PIN_LED_RED */
#endif /* GPIO_PIN_LED */
