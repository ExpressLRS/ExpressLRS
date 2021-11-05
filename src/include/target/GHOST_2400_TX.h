#if !defined(DEVICE_NAME)
    #define DEVICE_NAME             "Ghost 24TX"
#endif
// There is some special handling for this target
#define TARGET_TX_GHOST

// Any device features
#if !defined(USE_OLED_SPI_SMALL)
    #define USE_OLED_SPI
#endif
#define Regulatory_Domain_ISM_2400

// GPIO pin definitions
#define GPIO_PIN_NSS                PA15
#define GPIO_PIN_BUSY               PB15
#define GPIO_PIN_DIO1               PB2
#define GPIO_PIN_MOSI               PA7
#define GPIO_PIN_MISO               PA6
#define GPIO_PIN_SCK                PA5
#define GPIO_PIN_RST                PB0
#define GPIO_PIN_TX_ENABLE          PA8  // Works on Lite
#define GPIO_PIN_RX_ENABLE          PB14 // Works on Lite
#define GPIO_PIN_ANT_CTRL_1         PA9
#define GPIO_PIN_ANT_CTRL_2         PB13
#define GPIO_PIN_RCSIGNAL_RX        PA10 // S.PORT (Only needs one wire )
#define GPIO_PIN_RCSIGNAL_TX        PB6  // Needed for CRSF libs but does nothing/not hooked up to JR module.
#define GPIO_PIN_LED_WS2812         PB6
#define GPIO_PIN_LED_WS2812_FAST    PB_6
#define GPIO_PIN_PA_ENABLE          PB11  // https://www.skyworksinc.com/-/media/SkyWorks/Documents/Products/2101-2200/SE2622L_202733C.pdf
#define GPIO_PIN_RF_AMP_DET         PA3  // Voltage detector pin
#define GPIO_PIN_BUZZER             PC13
#define GPIO_PIN_OLED_CS            PC14
#define GPIO_PIN_OLED_RST           PB12
#define GPIO_PIN_OLED_DC            PC15
#define GPIO_PIN_OLED_MOSI          PB5
#define GPIO_PIN_OLED_SCK           PB3

// Output Power
#define MinPower                    PWR_10mW
#define MaxPower                    PWR_250mW
#define POWER_OUTPUT_VALUES         {-16,-14,-11,-8,-4}
