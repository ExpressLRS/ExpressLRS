#ifndef DEVICE_NAME
#define DEVICE_NAME "RadioMstr Zorro"
#endif
// Copied from DupleTX

// Any device features
#define RADIO_SX1280
#define USE_SX1280_DCDC
#define USE_SKY85321
#define SKY85321_PDET_SLOPE     0.035
#define SKY85321_PDET_INTERCEPT 2.4
#define USE_TX_BACKPACK

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
#define GPIO_PIN_RCSIGNAL_RX    3
#define GPIO_PIN_RCSIGNAL_TX    1

#define GPIO_PIN_PA_PDET        35

// Backpack pins
#define GPIO_PIN_DEBUG_RX       16
#define GPIO_PIN_DEBUG_TX       17
#define GPIO_PIN_BACKPACK_EN    25
#define GPIO_PIN_BACKPACK_BOOT  15

// Output Power
#define MinPower PWR_10mW
#define MaxPower PWR_250mW
#define POWER_OUTPUT_VALUES {-17,-13,-9,-6,-2}
