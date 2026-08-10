#pragma once
#if defined(PLATFORM_ESP32)

#include "device.h"
#include "gyro_config.h"

extern bool gyroDetected();

extern GyroConfig *gyroConfig;
extern device_t Gyro_device;
#endif