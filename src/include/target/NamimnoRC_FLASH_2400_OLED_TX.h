#define DEVICE_NAME "Namimno Flash OLED"

// Features
#define USE_TX_BACKPACK
#define USE_OLED

// GPIO pin definitions
#define GPIO_PIN_RST            21
#define GPIO_PIN_BUSY           22
#define GPIO_PIN_DIO1           17
#define GPIO_PIN_DIO2           16
#define GPIO_PIN_NSS            5
#define GPIO_PIN_MOSI           23
#define GPIO_PIN_MISO           19
#define GPIO_PIN_SCK            18
// SKY65383-11 front end control
#define GPIO_PIN_RX_ENABLE      32    // CRX
#define GPIO_PIN_TX_ENABLE      33    // CTX
#define GPIO_PIN_PA_ENABLE      25    // CSD

/* OLED definition */
#define GPIO_PIN_OLED_CS        12
#define GPIO_PIN_OLED_RST       27
#define GPIO_PIN_OLED_DC        26
#define GPIO_PIN_OLED_MOSI      15
#define GPIO_PIN_OLED_SCK       14

/* Joystick definition */
#define GPIO_PIN_JOYSTICK       35

/* S.Port input signal */
#define GPIO_PIN_RCSIGNAL_RX    13
#define GPIO_PIN_RCSIGNAL_TX    13
#define GPIO_PIN_FAN_EN         2

/* Backpack logger connection */
#define GPIO_PIN_DEBUG_RX       3
#define GPIO_PIN_DEBUG_TX       1

/* WS2812 led */
#define GPIO_PIN_LED_WS2812     4

// Output Power
#define MinPower                PWR_25mW
#define MaxPower                PWR_1000mW
#define POWER_OUTPUT_VALUES     {-18,-13,-10,-5,-2,3}

#define Regulatory_Domain_ISM_2400 1
