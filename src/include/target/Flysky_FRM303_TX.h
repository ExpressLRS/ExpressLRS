#define DEVICE_NAME          "FLYSKY FRM303"

// There is some special handling for this target
#define TARGET_TX_FLYSKY
#define TARGET_USE_EEPROM       1
#define TARGET_EEPROM_ADDR      0x50
#define TARGET_EEPROM_SIZE      kbits_256
#define TARGET_EEPROM_PAGE_SIZE 64
#define BACKPACK_LOGGING_BAUD   400000

// GPIO pin definitions
#define GPIO_PIN_NSS            PA15
#define GPIO_PIN_DIO1           PB10
#define GPIO_PIN_MOSI           PB5
#define GPIO_PIN_MISO           PB4
#define GPIO_PIN_SCK            PB3
#define GPIO_PIN_RST            PA0
#define GPIO_PIN_SDA            PB14
#define GPIO_PIN_SCL            PB13
#define GPIO_PIN_RX_ENABLE      PB1
#define GPIO_PIN_TX_ENABLE      PA4
#define GPIO_PIN_PA_ENABLE      PA5 
#define GPIO_PIN_BUSY           PB6
#define GPIO_PIN_RCSIGNAL_RX    PA3 // UART2 Ext Module connector IRM303 PA3
#define GPIO_PIN_RCSIGNAL_TX    PA2 // UART2 Ext Module connector IRM303 PA2
#define GPIO_PIN_LED_RED        PB7 // Red LED (active low)
#define GPIO_LED_RED_INVERTED   1
#define GPIO_PIN_LED_GREEN      PB8 // Green LED (active low)
#define GPIO_LED_GREEN_INVERTED 1
#define GPIO_PIN_LED_BLUE       PB9 // Blue LED (active low)
#define GPIO_LED_BLUE_INVERTED  1
#define GPIO_PIN_BUZZER         PB11
//#define GPIO_PIN_FIVE_WAY_INPUT1 PA7 // Useless without LCD

#define GPIO_PIN_DEBUG_RX       PA10 // UART1 Data Out IRM301
#define GPIO_PIN_DEBUG_TX       PA9 // UART1 Data Out IRM301

// Power output
#define MinPower                PWR_10mW
#define HighPower               PWR_100mW
#define MaxPower                PWR_250mW
#define POWER_OUTPUT_VALUES     {-15,-11,-7,-1,6}

#define Regulatory_Domain_ISM_2400 1
