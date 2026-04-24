#pragma once

#if defined(PLATFORM_ESP32) && defined(TARGET_RX)
#include "device.h"

extern device_t SerialUpdate_device;
#endif