#pragma once

#if defined(TARGET_RX)

#include "common.h"
#include "device.h"

void Vbat_enableSlowUpdate(bool enable);
void Vbat_setCalibrationActive(bool active);

extern uint32_t vbatMilliVolts; // latest primary Vbat reading, 0 if none

extern device_t AnalogVbat_device;

#endif
