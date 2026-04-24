#pragma once

#include "common.h"
#include "device.h"

#if defined(USE_ANALOG_VBAT)
void Vbat_enableSlowUpdate(bool enable);

extern device_t AnalogVbat_device;
#endif