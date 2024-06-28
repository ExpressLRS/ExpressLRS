#define DEVICE_NAME          "FLYSKY EL18"

// There is some special handling for this target
#define TARGET_TX_FLYSKY
#define TARGET_USE_EEPROM       1
#define TARGET_EEPROM_ADDR      0x50
#define TARGET_EEPROM_SIZE      kbits_256
#define TARGET_EEPROM_PAGE_SIZE 64
#define BACKPACK_LOGGING_BAUD   400000

#define SERIAL_USE_DMA
#define SERIAL_INSTANCE         1

#define FLYSKY_USBD_PID         0xf004

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
#define GPIO_PIN_ANT_CTRL       PA6  // R Antenna SW
#define GPIO_PIN_ANT_CTRL_COMPL PA5  // L Antenna SW
#define GPIO_PIN_H_POWER        PB8
#define GPIO_PIN_L_POWER        PB7
#define GPIO_PIN_BUSY           PB6
#define GPIO_PIN_RCSIGNAL_RX    PA3  // UART2 to radio connector EL18 IRM301 PA3
#define GPIO_PIN_RCSIGNAL_TX    PA2  // UART2 to radio connector EL18 IRM301 PA2
#define GPIO_PIN_DEBUG_RX       PA10 // UART1 Data Out EL18 IRM301
#define GPIO_PIN_DEBUG_TX       PA9  // UART1 Data Out EL18 IRM301

// Power output
#define MinPower                PWR_10mW
#define HighPower               PWR_100mW
#define MaxPower                PWR_100mW
#define DefaultPower            PWR_100mW
#define POWER_OUTPUT_VALUES     {2,6,9,13} // PA gain is 8dbm

#define Regulatory_Domain_ISM_2400 1
