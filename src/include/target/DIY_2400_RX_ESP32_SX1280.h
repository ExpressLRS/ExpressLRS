#ifndef DEVICE_NAME
#define DEVICE_NAME "ESP32 RX 2400"
#endif

#define USE_ANALOG_VBAT

// Radio GPIO pin definitions
#define GPIO_PIN_NSS            10
#define GPIO_PIN_BUSY           36
#define GPIO_PIN_DIO1           39
#define GPIO_PIN_MOSI           23
#define GPIO_PIN_MISO           19
#define GPIO_PIN_SCK            18
#define GPIO_PIN_RST            9

// VTx pins and Vpd setpoints
#define GPIO_PIN_SPI_VTX_SCK    14
#define GPIO_PIN_SPI_VTX_MOSI   13
#define GPIO_PIN_SPI_VTX_MISO   37  // We have to attach MISO even though we're not using it, so we attach it to an unused input-only pin
#define GPIO_PIN_SPI_VTX_NSS    15
#define GPIO_PIN_RF_AMP_VREF    26
#define GPIO_PIN_RF_AMP_PWM     25
#define GPIO_PIN_RF_AMP_VPD     A7  // 35
#define VPD_VALUES_25MW         {460, 470, 505, 505} // To be calibrated per target.
#define VPD_VALUES_100MW        {830, 840, 890, 895} // To be calibrated per target.

// PWM Pins
#define GPIO_PIN_PWM_OUTPUTS    {4, 5, 12, 21, 22, 27, 32, 33}
#define GPIO_ANALOG_VBAT        A6  // 34

// Vbat = (adc - ANALOG_VBAT_OFFSET) / ANALOG_VBAT_SCALE
// OFFSET is needed becauae ESPs don't go down to 0 even if their ADC is grounded
#if !defined(ANALOG_VBAT_OFFSET)
#define ANALOG_VBAT_OFFSET      12
#endif
#if !defined(ANALOG_VBAT_SCALE)
#define ANALOG_VBAT_SCALE       410
#endif

// RGB LED status and boot
#define GPIO_PIN_LED_WS2812     2
#define WS2812_PIXEL_COUNT      2
#define WS2812_STATUS_LEDS      {0}
#define WS2812_VTX_STATUS_LEDS  {1}
#define WS2812_BOOT_LEDS        {0, 1}

// Output Power
#define POWER_OUTPUT_FIXED      13 // MAX power for 2400 RXes that don't have PA is 12.5dbm
