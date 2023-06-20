#pragma once

#include "common.h"
#include "device.h"

#if defined(USE_ANALOG_VBAT)
enum VbatUpdateRate_e { vurNormal, vurSlow, vurDisabled };

void Vbat_setUpdateRate(VbatUpdateRate_e rate);

extern device_t AnalogVbat_device;
#endif