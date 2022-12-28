#pragma once

#include "device.h"

#if defined(PLATFORM_ESP32)
extern device_t BLET_device;
void BluetoothTelemetryUpdateValues(uint8_t *data);
void BluetoothTelemetryShutdown();
#define HAS_BLET
#endif