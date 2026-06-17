#pragma once

#if defined(TARGET_RX)

#include "common.h"
#include "device.h"

void Vbat_enableSlowUpdate(bool enable);
void Vbat_setCalibrationActive(bool active);

extern device_t AnalogVbat_device;

#endif
