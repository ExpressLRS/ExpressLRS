// This target extends the GHOST_2400_TX target
#ifndef DEVICE_NAME
#define DEVICE_NAME              "Ghost 24TX Lite"
#endif

// There is some special handling for this target
#define TARGET_TX_GHOST_LITE
#define USE_OLED_SPI_SMALL

#undef JOY_ADC_VALUES
/* Joystick values              {UP, DOWN, LEFT, RIGHT, ENTER, IDLE}*/
#define JOY_ADC_VALUES          {182, 325, 461, 512, 92, 1021}