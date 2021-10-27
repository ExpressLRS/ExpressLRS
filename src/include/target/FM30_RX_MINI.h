#if defined(RX_AS_TX)
    #define TX_DEVICE_NAME          "SIYI FR Mini"
    // There is some special handling for this target
    #define TARGET_TX_FM30_MINI
#else
    // There is some special handling for this target
    #define TARGET_RX_FM30_MINI
#endif

// GPIO pin definitions
#define GPIO_PIN_NSS                PA15
#define GPIO_PIN_BUSY               PE9
#define GPIO_PIN_DIO1               PE8
#define GPIO_PIN_MOSI               PB5
#define GPIO_PIN_MISO               PB4
#define GPIO_PIN_SCK                PB3
#define GPIO_PIN_RST                PB2
#define GPIO_PIN_TX_ENABLE          PD8 // CTX on SE2431L
#define GPIO_PIN_LED_RED            PB6
#define GPIO_PIN_LED_GREEN          PB7
#define GPIO_PIN_DEBUG_RX           PA10 // UART1
#define GPIO_PIN_DEBUG_TX           PA9  // UART1
#define GPIO_LED_RED_INVERTED       1
#define GPIO_LED_GREEN_INVERTED     1
#define GPIO_PIN_RCSIGNAL_TX        PA2 // UART2 NOTE: Not the "OUT" pinheader pad
#if defined(RX_AS_TX)
    #define GPIO_PIN_RCSIGNAL_RX    PA2 // UART2 (half duplex)
    #define GPIO_PIN_ANT_CTRL_2     PA8 // Low for left, high for right
#else
    #define GPIO_PIN_RCSIGNAL_RX    PA3 // UART2
    #define GPIO_PIN_ANTENNA_SELECT PA8 // Low for left, high for right
#endif
// Unused pins
#define GPIO_PIN_UART1TX_INVERT PF6

// Power output
#if defined(RX_AS_TX)
    #define MinPower                PWR_10mW
    #define HighPower               PWR_100mW
    #define MaxPower                PWR_250mW
    #define POWER_OUTPUT_VALUES     {-15,-11,-7,-1,6}
#else
    #ifdef UNLOCK_HIGHER_POWER
        #define POWER_OUTPUT_FIXED 6  // 250mW (uses values as above)
    #else
        #define POWER_OUTPUT_FIXED -1 // 100mW (uses values as above)
    #endif
#endif
