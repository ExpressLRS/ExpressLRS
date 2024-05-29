#define DEVICE_NAME          "FLYSKY IRM301"

// There is some special handling for this target
#define TARGET_TX_FLYSKY_IRM301

#define BACKPACK_LOGGING_BAUD 400000

// GPIO pin definitions
#define GPIO_PIN_NSS            PA15
#define GPIO_PIN_DIO1           PB10
#define GPIO_PIN_MOSI           PB5
#define GPIO_PIN_MISO           PB4
#define GPIO_PIN_SCK            PB3
#define GPIO_PIN_RST            PA0
#define GPIO_PIN_RX_ENABLE      PB1
#define GPIO_PIN_TX_ENABLE      PA4 
#define GPIO_PIN_ANT_CTRL       PA6 // R Antenna SW
#define GPIO_PIN_ANT_CTRL_COMPL PA5 // L Antenna SW
#define GPIO_PIN_H_POWER        PB8
#define GPIO_PIN_L_POWER        PB7
#define GPIO_PIN_BUSY           PB6
#define GPIO_PIN_RCSIGNAL_RX    PA3 // UART2 Ext Module connector IRM301 PA3
#define GPIO_PIN_RCSIGNAL_TX    PA2 // UART2 Ext Module connector IRM301 PA2
//#define GPIO_PIN_RCSIGNAL_RX    PA10 // UART1 Sport Ext Module connector IRM301
//#define GPIO_PIN_RCSIGNAL_TX    PA9  // UART1 Sport Ext Module connector IRM301
//#define GPIO_PIN_LED_RED        PB12 // Red LED (active low)
//#define GPIO_LED_RED_INVERTED   1
//#define GPIO_PIN_LED_GREEN      PB13 // Green LED (active low)
//#define GPIO_LED_GREEN_INVERTED 1
//#define GPIO_PIN_LED_BLUE       PB14 // Blue LED (active low)
//#define GPIO_LED_BLUE_INVERTED  1
//#define GPIO_PIN_BUTTON         PB9 // active low
//#define GPIO_BUTTON_INVERTED    1
#define GPIO_PIN_DEBUG_RX       PA10 // UART1 Data Out IRM301
#define GPIO_PIN_DEBUG_TX       PA9 // UART1 Data Out IRM301
//#define GPIO_PIN_DEBUG_RX       PA3 // UART2 FTR10 connector
//#define GPIO_PIN_DEBUG_TX       PA2 // UART2 FTR10 connector

// Power output
#define MinPower                PWR_10mW
#define HighPower               PWR_100mW
#define MaxPower                PWR_250mW
#define POWER_OUTPUT_VALUES     {-15,-11,-7,-1,6}

#define Regulatory_Domain_ISM_2400 1
