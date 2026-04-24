#pragma once

#include "device.h"

#if defined(GPIO_PIN_LED_WS2812)
extern device_t RGB_device;
#define HAS_RGB
#endif

#if defined(GPIO_PIN_LED) \
    || defined(GPIO_PIN_LED_RED) \
    || defined(GPIO_PIN_LED_GREEN) \
    || defined(GPIO_PIN_LED_BLUE)
extern device_t LED_device;
#define HAS_LED
#endif