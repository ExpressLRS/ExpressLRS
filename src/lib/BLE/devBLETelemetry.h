#pragma once

#include "device.h"

#if defined(PLATFORM_ESP32)
extern device_t BLET_device;
void BluetoothTelemetryUpdateValues(const uint8_t *data);
void BluetoothTelemetryShutdown();
#define HAS_BLETELEMETRY
#endif
