#define DEVICE_NAME "ELRS 2400RX"
// GPIO pin definitions
#define GPIO_PIN_NSS            15
#define GPIO_PIN_BUSY           5
#define GPIO_PIN_DIO1           4
#define GPIO_PIN_MOSI           13
#define GPIO_PIN_MISO           12
#define GPIO_PIN_SCK            14
#define GPIO_PIN_RST            2
#define GPIO_PIN_LED_RED        16 // LED_RED on TX, copied to LED on RX
#if defined(USE_DIVERSITY)
#define GPIO_PIN_ANTENNA_SELECT 0 // Low = Ant1, High = Ant2, pulled high by external resistor
#endif

#define Regulatory_Domain_ISM_2400 1

// VTx pins and Vpd setpoints
#define GPIO_PIN_SPI_VTX_NSS    9
#define GPIO_PIN_RF_AMP_VREF    10
#define GPIO_PIN_RF_AMP_PWM     0
#define GPIO_PIN_RF_AMP_VPD     A0
#define VPD_VALUES              {200, 350, 400, 430, 700} // {0, 10, 25, 100,  YOLO} mW.  To be calibrated per target.
