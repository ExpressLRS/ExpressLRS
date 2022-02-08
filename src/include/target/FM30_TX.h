#ifndef DEVICE_NAME
#define DEVICE_NAME          "SIYI FM30"
#endif

// There is some special handling for this target
#define TARGET_TX_FM30
#define RADIO_SX1280
#define USE_SX1280_DCDC

// GPIO pin definitions
#define GPIO_PIN_NSS            PB12
#define GPIO_PIN_DIO1           PB8
#define GPIO_PIN_MOSI           PB15
#define GPIO_PIN_MISO           PB14
#define GPIO_PIN_SCK            PB13
#define GPIO_PIN_RST            PB3
#define GPIO_PIN_TX_ENABLE      PB9 // CTX on SE2431L
#define GPIO_PIN_ANT_CTRL       PB4 // Low for left (stock), high for right (empty)
#define GPIO_PIN_RCSIGNAL_RX    PA10 // UART1
#define GPIO_PIN_RCSIGNAL_TX    PA9  // UART1
#define GPIO_PIN_BUFFER_OE      PB7
#define GPIO_PIN_BUFFER_OE_INVERTED 0
#define GPIO_PIN_LED_RED        PB2 // Right Red LED (active low)
#define GPIO_LED_RED_INVERTED   1
#define GPIO_PIN_LED_GREEN      PA7 // Left Green LED (active low)
#define GPIO_LED_GREEN_INVERTED 1
#define GPIO_PIN_BUTTON         PB0 // active low
#define GPIO_PIN_DEBUG_RX       PA3 // UART2 (bluetooth)
#define GPIO_PIN_DEBUG_TX       PA2 // UART2 (bluetooth)
#define GPIO_PIN_BLUETOOTH_EN   PA8 // Bluetooth power on (active low)
// GPIO not currently used (but initialized)
#define GPIO_PIN_LED_RED_GREEN  PB1 // Right Green LED (active low)
#define GPIO_PIN_LED_GREEN_RED  PA15 // Left Red LED (active low)
#define GPIO_PIN_UART3RX_INVERT PB5 // Standalone inverter
#define GPIO_PIN_UART1RX_INVERT PB6 // XOR chip
#define GPIO_PIN_DIP1           PA0 // Rotary Switch 0001
#define GPIO_PIN_DIP2           PA1 // Rotary Switch 0010
#define GPIO_PIN_DIP3           PA4 // Rotary Switch 0100
#define GPIO_PIN_DIP4           PA5 // Rotary Switch 1000
//#define GPIO_PIN_RX_ENABLE    UNDEF_PIN
//#define GPIO_PIN_BUZZER       UNDEF_PIN
//#define GPIO_PIN_FAN_EN       UNDEF_PIN

// Power output
#define MinPower                PWR_10mW
#define HighPower               PWR_100mW
#define MaxPower                PWR_250mW
#define POWER_OUTPUT_VALUES     {-15,-11,-7,-1,6}
