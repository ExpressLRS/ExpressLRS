#define DEVICE_NAME "AION T-Pro TX"

// Any device features
#define USE_SX1280_DCDC

// GPIO pin definitions
#define GPIO_PIN_NSS            5
#define GPIO_PIN_BUSY           21
#define GPIO_PIN_DIO1           4
#define GPIO_PIN_DIO2           22
#define GPIO_PIN_MOSI           23
#define GPIO_PIN_MISO           19
#define GPIO_PIN_SCK            18
#define GPIO_PIN_RST            14
#define GPIO_PIN_RX_ENABLE      27
#define GPIO_PIN_TX_ENABLE      26
#define GPIO_PIN_PA_ENABLE      25
#define GPIO_PIN_RCSIGNAL_RX    3
#define GPIO_PIN_RCSIGNAL_TX    1

// Backpack pins
#define GPIO_PIN_DEBUG_RX       13
#define GPIO_PIN_DEBUG_TX       12

// Output Power
#define MinPower                PWR_25mW
#define MaxPower                PWR_1000mW
#define POWER_OUTPUT_VALUES     {-18,-13,-10,-5,-2,3}

#define Regulatory_Domain_ISM_2400 1
