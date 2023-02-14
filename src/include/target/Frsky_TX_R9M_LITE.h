#ifndef DEVICE_NAME
#define DEVICE_NAME "FrSky R9M Lite"
#endif

#define TARGET_USE_EEPROM           1
#define TARGET_EEPROM_ADDR          0x51

// GPIO pin definitions
#define GPIO_PIN_RFswitch_CONTROL   PC13  // need to confirm  //HIGH = RX, LOW = TX

#define GPIO_PIN_NSS                PB12
#define GPIO_PIN_DIO0               PC15
#define GPIO_PIN_MOSI               PB15
#define GPIO_PIN_MISO               PB14
#define GPIO_PIN_SCK                PB13
#define GPIO_PIN_RST                PC14
#define GPIO_PIN_RX_ENABLE          PC13 //PB3 // need to confirm
#define GPIO_PIN_SDA                PB7
#define GPIO_PIN_SCL                PB6
#define GPIO_PIN_RCSIGNAL_RX        PB11 // not yet confirmed
#define GPIO_PIN_RCSIGNAL_TX        PB10 // not yet confirmed
#define GPIO_PIN_LED_RED            PA1 // Red LED // not yet confirmed
#define GPIO_PIN_LED_GREEN          PA4 // Green LED // not yet confirmed

#define GPIO_PIN_DEBUG_RX           PA3 // confirmed
#define GPIO_PIN_DEBUG_TX           PA2 // confirmed

#define GPIO_PIN_BUFFER_OE          PA5  //CONFIRMED
#define GPIO_PIN_BUFFER_OE_INVERTED 0

// Output Power
#define MinPower                    PWR_10mW
#define MaxPower                    PWR_100mW
#define HighPower                   PWR_100mW
#define DefaultPower			    PWR_50mW
#define POWER_OUTPUT_VALUES         {8,11,15,20}
