#ifndef DEVICE_NAME
#define DEVICE_NAME "FrSky R9M Lt Pro"
#endif

#define RADIO_SX127X
#define TARGET_USE_EEPROM           1
#define TARGET_EEPROM_ADDR          0x51

// GPIO pin definitions
#define GPIO_PIN_RFamp_APC1         PA4  //2.7V
#define GPIO_PIN_RFamp_APC2         PA5
#define GPIO_PIN_RFswitch_CONTROL   PA6  // confirmed  //HIGH = RX, LOW = TX

#define GPIO_PIN_NSS                PB12 // confirmed
#define GPIO_PIN_DIO0               PA8  // confirmed
#define GPIO_PIN_MOSI               PB15
#define GPIO_PIN_MISO               PB14
#define GPIO_PIN_SCK                PB13
#define GPIO_PIN_RST                PA9  // NRESET
#define GPIO_PIN_RX_ENABLE          GPIO_PIN_RFswitch_CONTROL
#define GPIO_PIN_SDA                PB9 // EEPROM ST M24C02-W
#define GPIO_PIN_SCL                PB8 // EEPROM ST M24C02-W
#define GPIO_PIN_RCSIGNAL_RX        PB11 // s.port inverter
#define GPIO_PIN_RCSIGNAL_TX        PB10 // s.port inverter
#define GPIO_PIN_LED_GREEN          PA15 // Green LED
//#define GPIO_PIN_LED_RED            PB3  // Red LED
#define GPIO_PIN_LED_RED            PB4  // Blue LED

#define GPIO_PIN_DEBUG_RX    	    PA3  // inverted UART JR
#define GPIO_PIN_DEBUG_TX      	    PA2  // inverted UART JR

#define GPIO_PIN_BUFFER_OE          PB2  //CONFIRMED
#define GPIO_PIN_BUFFER_OE_INVERTED 1
#define GPIO_PIN_VRF1			    PA7  // 26SU Switch RF1
#define GPIO_PIN_VRF2			    PB1  // 26SU Switch RF2
#define GPIO_PIN_SWR			    PA0  // SWR ADC1_IN1

// Output Power
#define MinPower                    PWR_10mW
#define HighPower                   PWR_250mW
#define MaxPower                    PWR_1000mW
#define POWER_OUTPUT_ANALOG
#define POWER_OUTPUT_VALUES         {600,770,950,1150,1480,2000,3500}
