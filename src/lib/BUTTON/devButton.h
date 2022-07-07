#pragma once

#include "device.h"
#include "config.h"

#if defined(GPIO_PIN_BUTTON)
    extern device_t Button_device;
    #define HAS_BUTTON
#endif
