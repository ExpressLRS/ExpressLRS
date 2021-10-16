#pragma once

#include "device.h"

#if defined(GPIO_PIN_LED_WS2812) && (GPIO_PIN_LED_WS2812 != UNDEF_PIN)
extern device_t RGB_device;
#define HAS_RGB
#endif

#if (defined(GPIO_PIN_LED) && (GPIO_PIN_LED != UNDEF_PIN)) \
    || (defined(GPIO_PIN_LED_RED) && (GPIO_PIN_LED_BLUE != UNDEF_PIN)) \
    || (defined(GPIO_PIN_LED_GREEN) && (GPIO_PIN_LED_GREEN != UNDEF_PIN)) \
    || (defined(GPIO_PIN_LED_BLUE) && (GPIO_PIN_LED_BLUE != UNDEF_PIN))
extern device_t LED_device;
#define HAS_LED
#endif