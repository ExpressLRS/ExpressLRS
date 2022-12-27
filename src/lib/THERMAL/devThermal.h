#pragma once

#if defined(GPIO_PIN_FAN_EN)
#define HAS_FAN
#endif

#if defined(HAS_THERMAL) || defined(HAS_FAN)
#include "device.h"

extern device_t Thermal_device;
#endif