#pragma once

#include "device.h"
#if defined(USE_OLED_SPI) || defined(USE_OLED_SPI_SMALL) || defined(USE_OLED_I2C) // This code will not be used if the hardware does not have a OLED display. Maybe a better way to blacklist it in platformio.ini?
#include "oledscreen.h"
#endif

extern device_t Screen_device;

