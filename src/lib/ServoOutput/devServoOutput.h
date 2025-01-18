#pragma once

#include "device.h"
#include "common.h"

#if defined(PLATFORM_ESP32)
#include "DShotRMT.h"
#endif

extern device_t ServoOut_device;

// Notify this unit that new channel data has arrived
void servoNewChannelsAvailable();
