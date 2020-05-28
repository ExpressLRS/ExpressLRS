#pragma once

#define EMPTY()
#define UNDEF_PIN (0xff)

/// General Features ///
#define LED_MAX_BRIGHTNESS 50 //0..255 for max led brightness

#define TIMER_OFFSET       200
#define TIMER_OFFSET_LIMIT 100

#define SX127X_SPI_SPEED 10000000

/******************************************************************************************/
/*                                     ESP TX CONFIGS                                     */
/******************************************************************************************/

#ifdef TARGET_TTGO_LORA_V1_AS_TX
#define GPIO_PIN_NSS         18
#define GPIO_PIN_DIO0        26
#define GPIO_PIN_DIO1        -1
#define GPIO_PIN_MOSI        27
#define GPIO_PIN_MISO        19
#define GPIO_PIN_SCK         5
#define GPIO_PIN_RST         14
#define GPIO_PIN_RX_ENABLE   -1
#define GPIO_PIN_TX_ENABLE   -1
#define GPIO_PIN_OLED_SDA    4
#define GPIO_PIN_OLED_SCK    15
#define GPIO_PIN_RCSIGNAL_RX 13
#define GPIO_PIN_RCSIGNAL_TX 13
//#define GPIO_PIN_LED         22 // 22 is not ok
#endif

#ifdef TARGET_TTGO_LORA_V1_AS_RX
#endif

#ifdef TARGET_TTGO_LORA_V2_AS_TX
#define GPIO_PIN_NSS         18
#define GPIO_PIN_DIO0        26
#define GPIO_PIN_DIO1        -1
#define GPIO_PIN_MOSI        27
#define GPIO_PIN_MISO        19
#define GPIO_PIN_SCK         5
#define GPIO_PIN_RST         14
#define GPIO_PIN_RX_ENABLE   -1
#define GPIO_PIN_TX_ENABLE   -1
#define GPIO_PIN_OLED_SDA    21
#define GPIO_PIN_OLED_SCK    22
#define GPIO_PIN_RCSIGNAL_RX 13
#define GPIO_PIN_RCSIGNAL_TX 13
#endif

#ifdef TARGET_TTGO_LORA_V2_AS_RX
#endif

#ifdef TARGET_EXPRESSLRS_PCB_TX_V3
#define GPIO_PIN_NSS         5
#define GPIO_PIN_DIO0        26
#define GPIO_PIN_DIO1        25
#define GPIO_PIN_MOSI        23
#define GPIO_PIN_MISO        19
#define GPIO_PIN_SCK         18
#define GPIO_PIN_RST         14
#define GPIO_PIN_RX_ENABLE   13
#define GPIO_PIN_TX_ENABLE   12
#define GPIO_PIN_OLED_SDA    -1
#define GPIO_PIN_OLED_SCK    -1
#define GPIO_PIN_RCSIGNAL_RX 2
#define GPIO_PIN_RCSIGNAL_TX 2 // so we don't have to solder the extra resistor, we switch rx/tx using gpio mux
#endif

#ifdef TARGET_ESP32_WROOM_RFM95
#define GPIO_PIN_NSS       5  // V_SPI_CS0
#define GPIO_PIN_DIO0      35 //26
#define GPIO_PIN_DIO1      34 //25
#define GPIO_PIN_MOSI      23 // V_SPI
#define GPIO_PIN_MISO      19 // V_SPI
#define GPIO_PIN_SCK       18 // V_SPI
#define GPIO_PIN_RST       -1 //14  // not connected ATM
#define GPIO_PIN_RX_ENABLE -1
#define GPIO_PIN_TX_ENABLE -1
#define GPIO_PIN_OLED_SDA  -1
#define GPIO_PIN_OLED_SCK  -1
// so we don't have to solder the extra resistor,
// we switch rx/tx using gpio mux
#define GPIO_PIN_RCSIGNAL_RX 2
#define GPIO_PIN_RCSIGNAL_TX 2
#endif

/******************************************************************************************/
/*                                     ESP RX CONFIGS                                     */
/******************************************************************************************/

#ifdef TARGET_EXPRESSLRS_PCB_RX_V3
#define GPIO_PIN_NSS         15
#define GPIO_PIN_DIO0        4
#define GPIO_PIN_DIO1        5
#define GPIO_PIN_MOSI        13
#define GPIO_PIN_MISO        12
#define GPIO_PIN_SCK         14
#define GPIO_PIN_RST         2
#define GPIO_PIN_LED         16
#define GPIO_PIN_RX_ENABLE   -1
#define GPIO_PIN_TX_ENABLE   -1
#define GPIO_PIN_OLED_SDA    -1
#define GPIO_PIN_OLED_SCK    -1
#define GPIO_PIN_RCSIGNAL_RX -1 //not relevant, can use only default for esp8266 or esp8285
#define GPIO_PIN_RCSIGNAL_TX -1
#define GPIO_PIN_BUTTON      0
#endif


/******************************************************************************************/
/*                                   STM32 RX CONFIGS                                     */
/******************************************************************************************/
#ifdef TARGET_RHF76_052
/*
    Other pins:
    PA15        LOW to enable default bootloader
*/

#define GPIO_PIN_NSS         PA4
#define GPIO_PIN_DIO0        PB10
#define GPIO_PIN_DIO1        PB2
#define GPIO_PIN_DIO2        PB0  // not used at the moment
#define GPIO_PIN_DIO3        PB1  // not used at the moment
#define GPIO_PIN_MOSI        PA7
#define GPIO_PIN_MISO        PA6
#define GPIO_PIN_SCK         PA5
#define GPIO_PIN_RST         PB11
/* PA1 and PA2 is used to selct HIGH or LOW RF band! */
/*
    LOW 0 : HIGH 0 = OFF
    LOW 1 : HIGH 0 = LOW BAND (433)
    LOW 0 : HIGH 1 = HIGH BAND (868 / 915)
*/
#define GPIO_SELECT_RFIO_HIGH PA2
#define GPIO_SELECT_RFIO_LOW  PA1
//#define GPIO_PIN_RX_ENABLE   -1
//#define GPIO_PIN_TX_ENABLE   -1
#define BUFFER_OE            -1
// USART1: TX=PA9, RX=PA10 (AF4) or TX=PB6, RX=PB7 (AF0)
#define GPIO_PIN_RCSIGNAL_RX PB7  // USART1, PIN23
#define GPIO_PIN_RCSIGNAL_TX PB6  // USART1, PIN22
#define GPIO_PIN_LED         PB4  // on board led (green), PIN16
#define GPIO_PIN_LED_GREEN   -1   //
#define GPIO_PIN_BUTTON      -1   // Note: pullup!
// USART2: TX=PA2, RX=PA3 or TX=PA14, RX=PA15. Both AF4
//#define GPIO_PIN_DEBUG_RX    PA3 // USART2, PIN??
//#define GPIO_PIN_DEBUG_TX    PA2 // USART2, PIN??

#define GPIO_PIN_RX_ENABLE -1
#define GPIO_PIN_TX_ENABLE -1
#endif // TARGET_RHF76_052

/******************************************************************************************/
/*                                        R9 CONFIGS                                      */
/******************************************************************************************/

/*
Credit to Jacob Walser (jaxxzer) for the pinout!!!
https://github.com/jaxxzer
*/
#ifdef TARGET_R9M_RX

#define GPIO_PIN_NSS         PB12
#define GPIO_PIN_DIO0        PA15
#define GPIO_PIN_DIO1        -1 //PA1 // NOT CORRECT!!! PIN STILL NEEDS TO BE FOUND BUT IS CURRENTLY UNUSED
#define GPIO_PIN_MOSI        PB15
#define GPIO_PIN_MISO        PB14
#define GPIO_PIN_SCK         PB13
#define GPIO_PIN_RST         PC14
#define GPIO_PIN_RX_ENABLE   -1
#define GPIO_PIN_TX_ENABLE   -1
#define GPIO_PIN_OLED_SDA    -1
#define GPIO_PIN_OLED_SCK    -1
#define BUFFER_OE            -1
#define GPIO_PIN_RCSIGNAL_RX PA10 // USART1
#define GPIO_PIN_RCSIGNAL_TX PA9  // USART1
#define GPIO_PIN_LED         PC1  // Red
#define GPIO_PIN_LED_GREEN   PB3  // Green - Currently unused
#define GPIO_PIN_BUTTON      PC13 // pullup e.g. LOW when pressed

#define GPIO_PIN_DEBUG_RX PA3 // confirmed, USART2
#define GPIO_PIN_DEBUG_TX PA2 // confirmed, USART2

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

#ifdef TARGET_R9M_TX
#define GPIO_PIN_NSS         PB12
#define GPIO_PIN_DIO0        PA15
#define GPIO_PIN_MOSI        PB15
#define GPIO_PIN_MISO        PB14
#define GPIO_PIN_SCK         PB13
#define GPIO_PIN_RST         PC14
#define GPIO_PIN_RX_ENABLE   -1
#define GPIO_PIN_TX_ENABLE   -1
#define GPIO_PIN_SDA         PB7
#define GPIO_PIN_SCL         PB6
#define GPIO_PIN_RCSIGNAL_RX PB11 // USART3 RX for S.Port
#define GPIO_PIN_RCSIGNAL_TX PB10 // USART3 TX for S.Port, needs BUFFER_OE
#define GPIO_PIN_LED_RED     PA11 // Red LED
#define GPIO_PIN_LED_GREEN   PA12 // Green LED
#define GPIO_PIN_LED         GPIO_PIN_LED_RED
#define GPIO_PIN_BUTTON PA8       // pullup e.g. LOW when pressed
#define GPIO_PIN_BUZZER PB1  // confirmed
#define GPIO_PIN_DIP1   PA12 // dip switch 1
#define GPIO_PIN_DIP2   PA11 // dip switch 2

//#define GPIO_PIN_DEBUG_RX PA3 // confirmed, USART2
//#define GPIO_PIN_DEBUG_TX PA2 // confirmed, USART2

#define BUFFER_OE     PA5  //CONFIRMED
#define SPORT         PB10 //CONFIRMED connected to tx3 and rx3 through 40ohn resistor. Needs BufferOE. inverted
#define GPIO_PIN_DIO1 PA1  //Not Needed, HEARTBEAT pin

#define GPIO_PIN_RFamp_APC1       PA6 //CONFIRMED SANDRO// APC2 is connected through a I2C dac and is handled elsewhere
#define GPIO_PIN_RFswitch_CONTROL PB3 //CONFIRMED SANDRO HIGH = RX, LOW = TX
// PwrAmp, RFControl, dodgy measurement with SDR, descending
// high low  -5
// low  low  -30
// low  high -40
// high high -40

class R9DAC;
extern R9DAC r9dac;
#endif
