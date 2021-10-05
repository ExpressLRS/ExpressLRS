#pragma once

#include "device.h"

#if defined(GPIO_PIN_BUTTON) && (GPIO_PIN_BUTTON != UNDEF_PIN)
extern device_t Button_device;
#define HAS_BUTTON
#endif
