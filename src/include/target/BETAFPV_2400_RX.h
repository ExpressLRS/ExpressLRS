#ifndef DEVICE_NAME
#define DEVICE_NAME "BFPV Nano 2G4RX"
#endif

#define USE_SX1280_DCDC

// GPIO pin definitions
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
#define GPIO_PIN_RX_ENABLE      9 //enable pa
#define GPIO_PIN_TX_ENABLE      10

// Output Power
#define MinPower            PWR_10mW
#define MaxPower            PWR_100mW
#define DefaultPower        PWR_100mW
#define POWER_OUTPUT_VALUES {-10,-6,-3,1} //has PA, use Power array
