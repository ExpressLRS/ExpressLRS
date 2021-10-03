#pragma once
#if !defined TARGET_NATIVE
#include <Arduino.h>
#endif

#define UNDEF_PIN (-1)

/// General Features ///
#define LED_MAX_BRIGHTNESS 50 //0..255 for max led brightness

/////////////////////////

#define WORD_ALIGNED_ATTR __attribute__((aligned(4)))

#ifdef PLATFORM_STM32
#define ICACHE_RAM_ATTR //nothing//
#else
#undef ICACHE_RAM_ATTR //fix to allow both esp32 and esp8266 to use ICACHE_RAM_ATTR for mapping to IRAM
#define ICACHE_RAM_ATTR IRAM_ATTR
#endif

#ifdef TARGET_TTGO_LORA_V1_AS_TX
#define GPIO_PIN_NSS 18
#define GPIO_PIN_DIO0 26
#define GPIO_PIN_MOSI 27
#define GPIO_PIN_MISO 19
#define GPIO_PIN_SCK 5
#define GPIO_PIN_RST 14
#define GPIO_PIN_OLED_SDA 4
#define GPIO_PIN_OLED_SCK 15
#define GPIO_PIN_OLED_RST 16
#define GPIO_PIN_RCSIGNAL_RX 13
#define GPIO_PIN_RCSIGNAL_TX 13
#define GPIO_PIN_LED 2
#define GPIO_PIN_BUTTON 0

#elif defined(TARGET_TTGO_LORA_V1_AS_RX)

#elif defined(TARGET_TTGO_LORA_V2_AS_TX)
#define GPIO_PIN_NSS 18
#define GPIO_PIN_DIO0 26
#define GPIO_PIN_MOSI 27
#define GPIO_PIN_MISO 19
#define GPIO_PIN_SCK 5
#define GPIO_PIN_RST 12 //was wrong 14
#define GPIO_PIN_OLED_SDA 21
#define GPIO_PIN_OLED_SCK 22
#define GPIO_PIN_OLED_RST U8X8_PIN_NONE
#define GPIO_PIN_RCSIGNAL_RX 13
#define GPIO_PIN_RCSIGNAL_TX 13
#define GPIO_PIN_BUTTON 39

#elif defined(TARGET_TTGO_LORA_V2_AS_RX)
 // not supported

#elif defined(TARGET_EXPRESSLRS_PCB_TX_V3)
#define GPIO_PIN_NSS 5
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

#elif defined(TARGET_EXPRESSLRS_PCB_TX_V3_LEGACY)
#define GPIO_PIN_BUTTON 36
#define RC_SIGNAL_PULLDOWN 4

#elif defined(TARGET_EXPRESSLRS_PCB_RX_V3)
#define GPIO_PIN_NSS 15
#if defined(TARGET_NAMIMNORC_ESP_RX) || defined(TARGET_RX_HUZZAH_RFM95W)
    #define GPIO_PIN_DIO0 5
    #define GPIO_PIN_DIO1 4
#else
    #define GPIO_PIN_DIO0 4
    #define GPIO_PIN_DIO1 5
#endif
#define GPIO_PIN_MOSI 13
#define GPIO_PIN_MISO 12
#define GPIO_PIN_SCK 14
#define GPIO_PIN_RST 2
#define GPIO_PIN_RCSIGNAL_RX -1 //only uses default uart pins so leave as -1
#define GPIO_PIN_RCSIGNAL_TX -1

#if defined(TARGET_RX_HUZZAH_RFM95W)
    #define GPIO_PIN_RCSIGNAL_RX -1 //only uses default uart pins so leave as -1
    #define GPIO_PIN_RCSIGNAL_TX -1
    #define GPIO_PIN_LED 0
    //#define GPIO_PIN_BUTTON -1
#else
    #define GPIO_PIN_RCSIGNAL_RX -1 //only uses default uart pins so leave as -1
    #define GPIO_PIN_RCSIGNAL_TX -1
    #define GPIO_PIN_LED 16
    #define GPIO_PIN_BUTTON 0
#endif

#define timerOffset -1

#elif defined(TARGET_R9M_RX)
/*
Credit to Jacob Walser (jaxxzer) for the pinout!!!
https://github.com/jaxxzer
*/
#define GPIO_PIN_NSS            PB12 //confirmed on SLIMPLUS, R900MINI
#define GPIO_PIN_DIO0           PA15 //confirmed on SLIMPLUS, R900MINI
#define GPIO_PIN_DIO1           PA1  // NOT CORRECT!!! PIN STILL NEEDS TO BE FOUND BUT IS CURRENTLY UNUSED
/////////////////////////////////////// NOT FOUND ON SLIMPLUS EITHER.
#define GPIO_PIN_MOSI           PB15 //confirmed on SLIMPLUS, R900MINI
#define GPIO_PIN_MISO           PB14 //confirmed on SLIMPLUS, R900MINI
#define GPIO_PIN_SCK            PB13 //confirmed on SLIMPLUS, R900MINI
#define GPIO_PIN_RST            PC14 //confirmed on SLIMPLUS, R900MINI
#define GPIO_PIN_SDA            PB7
#define GPIO_PIN_SCL            PB6

#if defined(USE_R9MM_R9MINI_SBUS)
    #define GPIO_PIN_RCSIGNAL_RX    PA3
    #define GPIO_PIN_RCSIGNAL_TX    PA2
#elif defined(TARGET_R9SLIM_RX)
    #define GPIO_PIN_RCSIGNAL_RX    PA3  // RX1 PIN OF CONNECTOR 1 ON SLIM
    #define GPIO_PIN_RCSIGNAL_TX    PA2  // TX1 PIN OF CONNECTOR 1 ON SLIM
#elif defined(TARGET_R9SLIMPLUS_RX)      // R9SLIMPLUS USES DUAL UART CONFIGURATION FOR TX1/RX1
    #define GPIO_PIN_RCSIGNAL_RX    PB11 // RX1 PIN OF CONNECTOR 1 ON SLIMPLUS
    #define GPIO_PIN_RCSIGNAL_TX    PA9  // TX1 PIN OF CONNECTOR 1 ON SLIMPLUS
#elif defined(TARGET_R900MINI_RX)
    #define GPIO_PIN_RCSIGNAL_RX    PA3 // convinient pin for direct chip solder
    #define GPIO_PIN_RCSIGNAL_TX    PA2 // convinient pin for direct chip solder
#else
    #define GPIO_PIN_RCSIGNAL_RX    PA10
    #define GPIO_PIN_RCSIGNAL_TX    PA9
#endif

#if defined(TARGET_R9MX_RX)
    #define GPIO_PIN_LED_RED        PB2 // Red
    #define GPIO_PIN_LED_GREEN      PB3 // Green
    #define GPIO_PIN_BUTTON         PB0  // pullup e.g. LOW when pressed
#elif defined(TARGET_R9SLIM_RX)
    #define GPIO_PIN_LED_RED        PA11 // Red
    #define GPIO_PIN_LED_GREEN      PA12 // Green
    #define GPIO_PIN_BUTTON         PC13  // pullup e.g. LOW when pressed
    /* PB3: RX = HIGH, TX = LOW */
    #define GPIO_PIN_RX_ENABLE      PB3
#elif defined(TARGET_R9SLIMPLUS_RX)
    #define GPIO_PIN_LED_RED        PA11 // Red
    #define GPIO_PIN_LED_GREEN      PA12 // Green
    #define GPIO_PIN_BUTTON         PC13  // pullup e.g. LOW when pressed
    /* PB3: RX = HIGH, TX = LOW */
    #define GPIO_PIN_RX_ENABLE      PB3
    /* PB9: antenna 1 (left) = HIGH, antenna 2 (right) = LOW
     * Note: Right Antenna is selected by default, LOW */
    #define GPIO_PIN_ANTENNA_SELECT PB9
#elif defined(TARGET_R900MINI_RX)
    #define GPIO_PIN_LED_RED        PA11 // Red
    #define GPIO_PIN_LED_GREEN      PA12 // Green
    #define GPIO_PIN_BUTTON         PC13 // pullup e.g. LOW when pressed
    // RF Switch: HIGH = RX, LOW = TX
    #define GPIO_PIN_RX_ENABLE      PB3
#else //R9MM_R9MINI
    #define GPIO_PIN_LED_RED        PC1  // Red
    #define GPIO_PIN_LED_GREEN      PB3  // Green
    #define GPIO_PIN_BUTTON         PC13 // pullup e.g. LOW when pressed
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

#elif defined(TARGET_R9M_TX) || defined(TARGET_TX_ES915TX)

#define GPIO_PIN_RFamp_APC1           PA6  //APC2 is connected through a I2C dac and is handled elsewhere
#define GPIO_PIN_RFswitch_CONTROL     PB3  //HIGH = RX, LOW = TX

#define GPIO_PIN_NSS            PB12
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


#define GPIO_PIN_BUFFER_OE      PA5  //CONFIRMED
#define GPIO_PIN_BUFFER_OE_INVERTED 0
#define GPIO_PIN_DIO1           PA1  //Not Needed, HEARTBEAT pin

#define DAC_I2C_ADDRESS         0b0001100
#define DAC_IN_USE              1

#elif defined(TARGET_R9M_LITE_TX)

#define GPIO_PIN_RFswitch_CONTROL     PC13  // need to confirm  //HIGH = RX, LOW = TX

#define GPIO_PIN_NSS            PB12
#define GPIO_PIN_DIO0           PC15
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

#define GPIO_PIN_BUFFER_OE      PA5  //CONFIRMED
#define GPIO_PIN_BUFFER_OE_INVERTED 0
#define MinPower PWR_10mW
#define MaxPower PWR_50mW
#define POWER_VALUES {8,11,15}

#elif defined(TARGET_R9M_LITE_PRO_TX)
#define GPIO_PIN_RFamp_APC1           PA4  //2.7V
#define GPIO_PIN_RFamp_APC2           PA5
#define GPIO_PIN_RFswitch_CONTROL     PA6  // confirmed  //HIGH = RX, LOW = TX

#define GPIO_PIN_NSS            PB12 // confirmed
#define GPIO_PIN_DIO0           PA8  // confirmed
#define GPIO_PIN_MOSI           PB15
#define GPIO_PIN_MISO           PB14
#define GPIO_PIN_SCK            PB13
#define GPIO_PIN_RST            PA9  // NRESET
#define GPIO_PIN_RX_ENABLE      GPIO_PIN_RFswitch_CONTROL
#define GPIO_PIN_SDA            PB9 // EEPROM ST M24C02-W
#define GPIO_PIN_SCL            PB8 // EEPROM ST M24C02-W
#define GPIO_PIN_RCSIGNAL_RX    PB11 // s.port inverter
#define GPIO_PIN_RCSIGNAL_TX    PB10 // s.port inverter
#define GPIO_PIN_LED_GREEN      PA15 // Green LED
//#define GPIO_PIN_LED_RED        PB3  // Red LED
#define GPIO_PIN_LED_RED        PB4  // Blue LED

#define GPIO_PIN_DEBUG_RX    	  PA3  // inverted UART JR
#define GPIO_PIN_DEBUG_TX      	PA2  // inverted UART JR

#define GPIO_PIN_BUFFER_OE      PB2  //CONFIRMED
#define GPIO_PIN_BUFFER_OE_INVERTED     1
#define GPIO_PIN_VRF1			        PA7  // 26SU Switch RF1
#define GPIO_PIN_VRF2			        PB1  // 26SU Switch RF2
#define GPIO_PIN_SWR			         PA0  // SWR ADC1_IN1

#elif defined(TARGET_RX_ESP8266_SX1280_V1) || defined(TARGET_TX_ESP8266_SX1280)
#define GPIO_PIN_NSS            15
#define GPIO_PIN_BUSY           5
#define GPIO_PIN_DIO1           4
#define GPIO_PIN_MOSI           13
#define GPIO_PIN_MISO           12
#define GPIO_PIN_SCK            14
#define GPIO_PIN_RST            2
//#define GPIO_PIN_RCSIGNAL_RX -1 // uses default uart pins
//#define GPIO_PIN_RCSIGNAL_TX -1
#define GPIO_PIN_LED_RED        16 // LED_RED on TX, copied to LED on RX
#if defined(TARGET_RX) && defined(USE_DIVERSITY)
#define GPIO_PIN_ANTENNA_SELECT 0 // Low = Ant1, High = Ant2, pulled high by external resistor
#else
#define GPIO_PIN_BUTTON         0
#endif
#define MinPower PWR_10mW
#define MaxPower PWR_10mW
#define POWER_VALUES {15}

#elif defined(TARGET_TX_ESP32_SX1280_V1)
#define GPIO_PIN_NSS 5
#define GPIO_PIN_BUSY 21
#define GPIO_PIN_DIO1 4
#define GPIO_PIN_MOSI 23
#define GPIO_PIN_MISO 19
#define GPIO_PIN_SCK 18
#define GPIO_PIN_RST 14
#define GPIO_PIN_RCSIGNAL_RX 13
#define GPIO_PIN_RCSIGNAL_TX 13
#define MinPower PWR_10mW
#define MaxPower PWR_25mW
#define POWER_VALUES {8, 13}

#elif defined(TARGET_RX_GHOST_ATTO_V1)
#define GPIO_PIN_NSS            PA15
#define GPIO_PIN_BUSY           PA3
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

#elif defined(TARGET_TX_GHOST) // GHOST TX FULL AND LITE
#define GPIO_PIN_NSS                PA15
#define GPIO_PIN_BUSY               PB15
#define GPIO_PIN_DIO1               PB2
#define GPIO_PIN_MOSI               PA7
#define GPIO_PIN_MISO               PA6
#define GPIO_PIN_SCK                PA5
#define GPIO_PIN_RST                PB0
#define GPIO_PIN_TX_ENABLE          PA8  // Works on Lite
#define GPIO_PIN_RX_ENABLE          PB14 // Works on Lite
#define GPIO_PIN_ANT_CTRL_1         PA9
#define GPIO_PIN_ANT_CTRL_2         PB13
#define GPIO_PIN_RCSIGNAL_RX        PA10 // S.PORT (Only needs one wire )
#define GPIO_PIN_RCSIGNAL_TX        PB6  // Needed for CRSF libs but does nothing/not hooked up to JR module.
#define GPIO_PIN_LED_WS2812         PB6
#define GPIO_PIN_LED_WS2812_FAST    PB_6
#define GPIO_PIN_PA_SE2622L_ENABLE  PB11  // https://www.skyworksinc.com/-/media/SkyWorks/Documents/Products/2101-2200/SE2622L_202733C.pdf
#define GPIO_PIN_RF_AMP_DET         PA3  // Voltage detector pin
#define GPIO_PIN_BUZZER             PC13
#define GPIO_PIN_OLED_CS            PC14
#define GPIO_PIN_OLED_RST           PB12
#define GPIO_PIN_OLED_DC            PC15
#define GPIO_PIN_OLED_MOSI          PB5
#define GPIO_PIN_OLED_SCK           PB3
#define timerOffset                 1
#define MinPower PWR_10mW
#define MaxPower PWR_250mW
#define POWER_VALUES {-16,-14,-11,-8,-4}

#elif defined(TARGET_TX_ESP32_E28_SX1280_V1) || defined(TARGET_TX_ESP32_LORA1280F27)
#define GPIO_PIN_NSS 5
#define GPIO_PIN_BUSY 21
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
#define GPIO_PIN_LED_WS2812 15
#define GPIO_PIN_FAN_EN 17
#if defined(TARGET_TX_ESP32_LORA1280F27)
#define MinPower PWR_10mW
#define MaxPower PWR_250mW
#define POWER_VALUES {-4,0,3,6,12}
#elif defined(TARGET_HappyModel_ES24TX_2400_TX)
#define MinPower PWR_10mW
#define MaxPower PWR_250mW
#define POWER_VALUES {-17,-13,-9,-6,-2}
#elif defined(TARGET_HappyModel_ES24TX_Slim_Pro_2400_TX)
#define MinPower PWR_25mW
#define MaxPower PWR_1000mW
#define POWER_VALUES {-18,-15,-12,-7,-4,2}
#elif defined(TARGET_HGLRC_Hermes_2400_TX)
#define MinPower PWR_10mW
#define MaxPower PWR_250mW
#define POWER_VALUES {-18,-15,-11,-8,-4}
#elif defined(TARGET_QK_JR_TX)
#define MinPower PWR_10mW
#define MaxPower PWR_250mW
#define POWER_VALUES {-15,-11,-8,-5,0}
#else
#define MinPower PWR_10mW
#define MaxPower PWR_250mW
#define POWER_VALUES {-15,-11,-8,-5,-1}
#endif

#elif defined(TARGET_SX1280_RX_CCG_NANO_v05)
#define GPIO_PIN_NSS         PA4
#define GPIO_PIN_MOSI        PA7
#define GPIO_PIN_MISO        PA6
#define GPIO_PIN_SCK         PA5

#define GPIO_PIN_DIO1        PA10
#define GPIO_PIN_RST         PB4
#define GPIO_PIN_BUSY        PA11

#define GPIO_PIN_RCSIGNAL_RX PB7  // USART1, AFAIO
#define GPIO_PIN_RCSIGNAL_TX PB6  // USART1, AFAIO

#define GPIO_PIN_LED_RED     PB5

#define timerOffset          1

#elif defined(TARGET_NAMIMNORC_TX)
/*
Designed by NamimnoRC
*/
#if TARGET_MODULE_2400
    #define GPIO_PIN_RST            PB4
    #define GPIO_PIN_BUSY           PB5
    #define GPIO_PIN_DIO1           PB6
    #define GPIO_PIN_DIO2           PB7
    #define GPIO_PIN_NSS            PA4
    #define GPIO_PIN_MOSI           PA7
    #define GPIO_PIN_MISO           PA6
    #define GPIO_PIN_SCK            PA5
    // SKY65383-11 front end control
    #define GPIO_PIN_RX_ENABLE      PA8     // CRX
    #define GPIO_PIN_TX_ENABLE      PA11    // CTX
    #define GPIO_PIN_PA_ENABLE      PA12    // CSD
#else // !TARGET_MODULE_2400
    #define GPIO_PIN_NSS            PB12
    #define GPIO_PIN_DIO0           PA15
    #define GPIO_PIN_MOSI           PB15
    #define GPIO_PIN_MISO           PB14
    #define GPIO_PIN_SCK            PB13
    #define GPIO_PIN_RST            PC14
    #define GPIO_PIN_RX_ENABLE      PB3  //HIGH = RX, LOW = TX
    /* DAC settings */
    #define GPIO_PIN_SDA            PB9
    #define GPIO_PIN_SCL            PB8
    #define DAC_I2C_ADDRESS         0b0001101
    #define DAC_IN_USE              1
#endif // TARGET_MODULE_2400

/* S.Port input signal */
#define GPIO_PIN_RCSIGNAL_RX    PB11 /* USART3 */
#define GPIO_PIN_RCSIGNAL_TX    PB10 /* USART3 */
#define GPIO_PIN_BUFFER_OE      PA1
#define GPIO_PIN_BUFFER_OE_INVERTED 1
#define GPIO_PIN_FAN_EN         PB1
/* Backpack logger connection */
#ifdef USE_ESP8266_BACKPACK
    #define GPIO_PIN_DEBUG_RX   PA10
    #define GPIO_PIN_DEBUG_TX   PA9
#else
    #define GPIO_PIN_DEBUG_RX   PA3
    #define GPIO_PIN_DEBUG_TX   PA2
#endif
/* WS2812 led */
#define GPIO_PIN_LED_WS2812      PB0
#define GPIO_PIN_LED_WS2812_FAST PB_0
#define MinPower PWR_25mW
#define MaxPower PWR_1000mW
#define POWER_VALUES {-18,-15,-12,-8,-5,3}

#elif defined(TARGET_NAMIMNORC_RX)
/*
Designed by NamimnoRC
*/
#if TARGET_MODULE_2400
    #define GPIO_PIN_RST        PB4
    #define GPIO_PIN_BUSY       PB5
    #define GPIO_PIN_DIO1       PB6
    #define GPIO_PIN_DIO2       PB7
    #define GPIO_PIN_NSS        PA4
    #define GPIO_PIN_MOSI       PA7
    #define GPIO_PIN_MISO       PA6
    #define GPIO_PIN_SCK        PA5
    #define GPIO_PIN_LED_RED    PA1
#else
    #define GPIO_PIN_RST        PC14
    #define GPIO_PIN_DIO0       PA15
    #define GPIO_PIN_DIO1       PA1
    #define GPIO_PIN_NSS        PB12
    #define GPIO_PIN_MOSI       PB15
    #define GPIO_PIN_MISO       PB14
    #define GPIO_PIN_SCK        PB13
    #define GPIO_PIN_LED_RED    PA11
    // RF Switch: LOW = RX, HIGH = TX
    #define GPIO_PIN_TX_ENABLE  PB3
#endif

#define GPIO_PIN_RCSIGNAL_RX    PA10
#define GPIO_PIN_RCSIGNAL_TX    PA9

#define timerOffset             1

#elif defined(TARGET_NATIVE)
#define IRAM_ATTR
#include "native.h"

#elif defined(TARGET_TX_FM30)
#define GPIO_PIN_NSS            PB12
#define GPIO_PIN_DIO1           PB8
#define GPIO_PIN_MOSI           PB15
#define GPIO_PIN_MISO           PB14
#define GPIO_PIN_SCK            PB13
#define GPIO_PIN_RST            PB3
//#define GPIO_PIN_RX_ENABLE    UNDEF_PIN
#define GPIO_PIN_TX_ENABLE      PB9 // CTX on SE2431L
#define GPIO_PIN_ANT_CTRL_2     PB4 // Low for left (stock), high for right (empty)
#define GPIO_PIN_RCSIGNAL_RX    PA10 // UART1
#define GPIO_PIN_RCSIGNAL_TX    PA9  // UART1
#define GPIO_PIN_BUFFER_OE      PB7
#define GPIO_PIN_BUFFER_OE_INVERTED 0
#define GPIO_PIN_LED_RED        PB2 // Right Red LED (active low)
#define GPIO_LED_RED_INVERTED   1
#define GPIO_PIN_LED_GREEN      PA7 // Left Green LED (active low)
#define GPIO_LED_GREEN_INVERTED 1
#define GPIO_PIN_BUTTON         PB0 // active low
//#define GPIO_PIN_BUZZER       UNDEF_PIN
#define GPIO_PIN_DIP1           PA0 // Rotary Switch 0001
#define GPIO_PIN_DIP2           PA1 // Rotary Switch 0010
//#define GPIO_PIN_FAN_EN       UNDEF_PIN
#define GPIO_PIN_DEBUG_RX       PA3 // UART2 (bluetooth)
#define GPIO_PIN_DEBUG_TX       PA2 // UART2 (bluetooth)
// GPIO not currently used (but initialized)
#define GPIO_PIN_LED_RED_GREEN  PB1 // Right Green LED (active low)
#define GPIO_PIN_LED_GREEN_RED  PA15 // Left Red LED (active low)
#define GPIO_PIN_UART3RX_INVERT PB5 // Standalone inverter
#define GPIO_PIN_BLUETOOTH_EN   PA8 // Bluetooth power on (active low)
#define GPIO_PIN_UART1RX_INVERT PB6 // XOR chip
#define MinPower PWR_10mW
#define MaxPower PWR_250mW
#define POWER_VALUES {-15,-11,-7,-1,6}

#elif defined(TARGET_RX_FM30_MINI) || defined(TARGET_TX_FM30_MINI)
#define GPIO_PIN_NSS            PA15
#define GPIO_PIN_BUSY           PE9
#define GPIO_PIN_DIO1           PE8
#define GPIO_PIN_MOSI           PB5
#define GPIO_PIN_MISO           PB4
#define GPIO_PIN_SCK            PB3
#define GPIO_PIN_RST            PB2
#define GPIO_PIN_TX_ENABLE      PD8 // CTX on SE2431L
#define GPIO_PIN_LED_RED        PB6
#define GPIO_PIN_LED_GREEN      PB7
#define GPIO_PIN_DEBUG_RX       PA10 // UART1
#define GPIO_PIN_DEBUG_TX       PA9  // UART1
#define GPIO_LED_RED_INVERTED   1
#define GPIO_LED_GREEN_INVERTED 1
#define GPIO_PIN_RCSIGNAL_TX    PA2 // UART2 NOTE: Not the "OUT" pinheader pad
    #if defined(TARGET_RX_FM30_MINI)
        #define GPIO_PIN_RCSIGNAL_RX    PA3 // UART2
        #define GPIO_PIN_ANTENNA_SELECT PA8 // Low for left, high for right
    #elif defined(TARGET_TX_FM30_MINI)
        #define GPIO_PIN_RCSIGNAL_RX    PA2 // UART2 (half duplex)
        #define GPIO_PIN_ANT_CTRL_2     PA8 // Low for left, high for right
    #endif
// Unused pins
#define GPIO_PIN_UART1TX_INVERT PF6
#define MinPower PWR_10mW
#define MaxPower PWR_250mW
#define POWER_VALUES {-15,-11,-7,-1,6}

#elif defined(TARGET_ES900TX)
#define GPIO_PIN_NSS            5
#define GPIO_PIN_DIO0           26
#define GPIO_PIN_DIO1           25
#define GPIO_PIN_MOSI           23
#define GPIO_PIN_MISO           19
#define GPIO_PIN_SCK            18
#define GPIO_PIN_RST            14
#define GPIO_PIN_RX_ENABLE      13
#define GPIO_PIN_TX_ENABLE      12
#define GPIO_PIN_RCSIGNAL_RX    2
#define GPIO_PIN_RCSIGNAL_TX    2 // so we don't have to solder the extra resistor, we switch rx/tx using gpio mux
#define GPIO_PIN_LED_WS2812     27
#define GPIO_PIN_FAN_EN         17
#define GPIO_PIN_RFamp_APC2     25

#elif defined(TARGET_TX_BETAFPV_2400_V1)
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
#define GPIO_PIN_LED_BLUE       17
#define GPIO_PIN_LED_GREEN      16
#define GPIO_PIN_BUTTON         25
#define MinPower PWR_10mW
#define MaxPower PWR_500mW
#define POWER_VALUES {-18,-15,-13,-9,-4,3}

#elif defined(TARGET_RX_BETAFPV_2400_V1)
#define GPIO_PIN_NSS            15
#define GPIO_PIN_BUSY           5
#define GPIO_PIN_DIO0           -1
#define GPIO_PIN_DIO1           4
#define GPIO_PIN_MOSI           13
#define GPIO_PIN_MISO           12
#define GPIO_PIN_SCK            14
#define GPIO_PIN_RST            2
#define GPIO_PIN_LED            16
#define GPIO_PIN_BUTTON         0
#define timerOffset             -1
#define GPIO_PIN_RX_ENABLE      9 //enable pa
#define GPIO_PIN_TX_ENABLE      10
#define MinPower PWR_10mW
#define MaxPower PWR_100mW
#define POWER_VALUES {-10,-6,-3,1}

#elif defined(TARGET_TX_BETAFPV_900_V1)
#define GPIO_PIN_NSS            5
#define GPIO_PIN_DIO0           4
#define GPIO_PIN_DIO1           2
#define GPIO_PIN_MOSI           23
#define GPIO_PIN_MISO           19
#define GPIO_PIN_SCK            18
#define GPIO_PIN_RST            14
#define GPIO_PIN_RX_ENABLE      27
#define GPIO_PIN_TX_ENABLE      26
#define GPIO_PIN_RCSIGNAL_RX    13
#define GPIO_PIN_RCSIGNAL_TX    13 // so we don't have to solder the extra resistor, we switch rx/tx using gpio mux
#define GPIO_PIN_LED_WA2812     -1
#define GPIO_PIN_LED_BLUE       17
#define GPIO_PIN_LED_GREEN      16
#define GPIO_PIN_BUTTON         25
#define MinPower PWR_100mW
#define MaxPower PWR_500mW
#define POWER_VALUES {0,3,8}

#elif defined(TARGET_RX_BETAFPV_900_V1)
#define GPIO_PIN_NSS            15
#define GPIO_PIN_DIO0           4
#define GPIO_PIN_DIO1           5
#define GPIO_PIN_MOSI           13
#define GPIO_PIN_MISO           12
#define GPIO_PIN_SCK            14
#define GPIO_PIN_RST            2
#define GPIO_PIN_LED            16
#define GPIO_PIN_BUTTON         0

#elif defined(TARGET_RX_MATEK_2400)
#define GPIO_PIN_NSS            15
#define GPIO_PIN_BUSY           5
#define GPIO_PIN_DIO1           4
#define GPIO_PIN_MOSI           13
#define GPIO_PIN_MISO           12
#define GPIO_PIN_SCK            14
#define GPIO_PIN_RST            2
#define GPIO_PIN_LED            16
#define GPIO_PIN_BUTTON         0
#define GPIO_PIN_TX_ENABLE      10
#ifdef USE_DIVERSITY
    #define GPIO_PIN_ANTENNA_SELECT 9
#endif

#else
#error "Unknown target!"
#endif

#if defined(PLATFORM_STM32)
#ifdef GPIO_PIN_LED_WS2812
#ifndef GPIO_PIN_LED_WS2812_FAST
#error "WS2812 support requires _FAST pin!"
#endif
#else
#define GPIO_PIN_LED_WS2812         UNDEF_PIN
#define GPIO_PIN_LED_WS2812_FAST    UNDEF_PIN
#endif
#endif

/* Set red led to default */
#ifndef GPIO_PIN_LED
#ifdef GPIO_PIN_LED_RED
#define GPIO_PIN_LED GPIO_PIN_LED_RED
#endif /* GPIO_PIN_LED_RED */
#endif /* GPIO_PIN_LED */

#ifndef GPIO_PIN_BUFFER_OE
#define GPIO_PIN_BUFFER_OE UNDEF_PIN
#endif
#ifndef GPIO_PIN_RST
#define GPIO_PIN_RST UNDEF_PIN
#endif
#ifndef GPIO_PIN_BUSY
#define GPIO_PIN_BUSY UNDEF_PIN
#endif
#ifndef GPIO_PIN_DIO0
#define GPIO_PIN_DIO0 UNDEF_PIN
#endif
#ifndef GPIO_PIN_DIO1
#define GPIO_PIN_DIO1 UNDEF_PIN
#endif
#ifndef GPIO_PIN_DIO2
#define GPIO_PIN_DIO2 UNDEF_PIN
#endif
#ifndef GPIO_PIN_PA_ENABLE
#define GPIO_PIN_PA_ENABLE UNDEF_PIN
#endif
#ifndef GPIO_BUTTON_INVERTED
#define GPIO_BUTTON_INVERTED 0
#endif
#ifndef GPIO_LED_RED_INVERTED
#define GPIO_LED_RED_INVERTED 0
#endif
#ifndef GPIO_LED_GREEN_INVERTED
#define GPIO_LED_GREEN_INVERTED 0
#endif
