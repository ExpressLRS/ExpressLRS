#define DEVICE_NAME          "FLYSKY FRM301"

// There is some special handling for this target
#define TARGET_TX_FLYSKY_FRM301

// GPIO pin definitions
#define GPIO_PIN_NSS            PA15
#define GPIO_PIN_DIO1           PC14
#define GPIO_PIN_MOSI           PB5
#define GPIO_PIN_MISO           PB4
#define GPIO_PIN_SCK            PB3
#define GPIO_PIN_RST            PA0
#define GPIO_PIN_RX_ENABLE      PB2
#define GPIO_PIN_TX_ENABLE      PA4 
#define GPIO_PIN_ANT_CTRL_1     PB10 // R Antenna SW
#define GPIO_PIN_ANT_CTRL_2     PB11 // L Antenna SW
#define GPIO_PIN_BUSY           PC13
//#define GPIO_PIN_RCSIGNAL_RX    PA3 // UART2 Ext Module connector FRM301
//#define GPIO_PIN_RCSIGNAL_TX    PA2  // UART2 Ext Module connector FRM301
#define GPIO_PIN_RCSIGNAL_RX    PA10 // UART1 Sport
#define GPIO_PIN_RCSIGNAL_TX    PA9  // UART1 Sport
//#define GPIO_PIN_BUFFER_OE      PB7
//#define GPIO_PIN_BUFFER_OE_INVERTED 0
#define GPIO_PIN_LED_RED        PB12 // Red LED (active low)
#define GPIO_LED_RED_INVERTED   1
#define GPIO_PIN_LED_GREEN      PB13 // Green LED (active low)
#define GPIO_LED_GREEN_INVERTED 1
#define GPIO_PIN_LED_BLUE       PB14 // Blue LED (active low)
#define GPIO_LED_BLUE_INVERTED  1
#define GPIO_PIN_BUTTON         PB9 // active low
//#define GPIO_PIN_BUZZER       UNDEF_PIN
//#define GPIO_PIN_DIP1           PA0 // Rotary Switch 0001
//#define GPIO_PIN_DIP2           PA1 // Rotary Switch 0010
//#define GPIO_PIN_FAN_EN       UNDEF_PIN
#define GPIO_PIN_DEBUG_RX       PB7 // UART1
#define GPIO_PIN_DEBUG_TX       PB6 // UART1
// GPIO not currently used (but initialized)
//#define GPIO_PIN_LED_RED_GREEN  PB1 // Right Green LED (active low)
//#define GPIO_PIN_LED_GREEN_RED  PA15 // Left Red LED (active low)
//#define GPIO_PIN_UART3RX_INVERT PB5 // Standalone inverter
//#define GPIO_PIN_BLUETOOTH_EN   PA8 // Bluetooth power on (active low)
//#define GPIO_PIN_UART1RX_INVERT PB6 // XOR chip

//check
// Power output
#define MinPower                PWR_10mW
#define HighPower               PWR_100mW
#define MaxPower                PWR_250mW
#define POWER_OUTPUT_VALUES     {-15,-11,-7,-1,6}

#define Regulatory_Domain_ISM_2400 1
