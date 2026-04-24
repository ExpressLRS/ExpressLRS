#pragma once

#if defined(HAS_THERMAL) || defined(HAS_FAN)
#include "device.h"
#include "CRSF.h"

extern device_t Thermal_device;
#endif