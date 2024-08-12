/*
Credit to Jacob Walser (jaxxzer) for the pinout!!!
https://github.com/jaxxzer
*/
#if !defined(TARGET_R9SLIM_RX)
    #define TARGET_USE_EEPROM               1
    #define TARGET_EEPROM_ADDR              0x50
#endif

#define GPIO_PIN_SDA            PB7  // EEPROM
#define GPIO_PIN_SCL            PB6  // EEPROM

// /*  Radio 1
#define GPIO_PIN_NSS            PA4  // RADIO 1 
#define GPIO_PIN_DIO0           PB5  // RADIO 1 
#define GPIO_PIN_MOSI           PA7  // RADIO 1 
#define GPIO_PIN_MISO           PA6  // RADIO 1 
#define GPIO_PIN_SCK            PA5  // RADIO 1 
#define GPIO_PIN_RST            PB2  // RADIO 1 
// */

 // Radio 2
#define GPIO_PIN_NSS_2            PB12  // RADIO 2 
#define GPIO_PIN_DIO0_2           PB4   // RADIO 2 
#define GPIO_PIN_MOSI_2           PB15  // RADIO 2 
#define GPIO_PIN_MISO_2           PB14  // RADIO 2 
#define GPIO_PIN_SCK_2            PB13  // RADIO 2 
#define GPIO_PIN_RST_2            PA15  // RADIO 2 


#define GPIO_PIN_RCSIGNAL_RX        PA10
#define GPIO_PIN_RCSIGNAL_TX        PA9
#define GPIO_PIN_DEBUG_RX           PA3
#define GPIO_PIN_DEBUG_TX           PA2
#ifndef DEVICE_NAME
    #define DEVICE_NAME "ModalAI M0139"
#endif

#define GPIO_PIN_LED_RED        PA12  // Red
#define GPIO_PIN_LED_GREEN      PB3   // Green
#define GPIO_PIN_BUTTON         PA1   // pullup e.g. LOW when pressed

// Output Power - Default to SX1276 max output
#define POWER_OUTPUT_FIXED 127 //MAX power for 900 RXes

// External pads
// PWM
#define GPIO_PIN_PWM_OUTPUTS (int[]){Ch1, Ch2, Ch3, Ch4}
#define GPIO_PIN_PWM_OUTPUTS_COUNT 4
#define Ch1    PB0     // TIM3 CH3
#define Ch2    PB1     // TIM3 CH4 
#define Ch3    PA8     // TIM1 CH1 
#define Ch4    PA11    // TIM1 CH4 

#define M0139
#define DUAL_RADIO
//#undef GPIO_PIN_NSS_2
//#define GPIO_PIN_NSS_2 UNDEF_PIN
#define SYSCLK_FREQ_72MHz
// #define GPIO_PIN_ANT_CTRL PB10 // Unused pin