#ifndef DEVICE_NAME
#define DEVICE_NAME "2.4G + VTx"
#endif

// Any device features
#define WS2812_IS_GRB

// GPIO pin definitions
#define GPIO_PIN_NSS            15
#define GPIO_PIN_DIO1           4
#define GPIO_PIN_MOSI           13
#define GPIO_PIN_MISO           12
#define GPIO_PIN_SCK            14
#define GPIO_PIN_RST            16
#define GPIO_PIN_ANTENNA_SELECT 5
#define GPIO_PIN_LED_WS2812     2  // Add 2 LEDs in series, one for the Rx and a second for VTx status.

// VTx pins and Vpd setpoints
#define GPIO_PIN_SPI_VTX_NSS    9
#define GPIO_PIN_RF_AMP_VREF    10
#define GPIO_PIN_RF_AMP_PWM     0
#define GPIO_PIN_RF_AMP_VPD     A0
#define VPD_VALUES_25MW         {460, 470, 505, 505} // To be calibrated per target.
#define VPD_VALUES_100MW        {830, 840, 890, 895} // To be calibrated per target.
