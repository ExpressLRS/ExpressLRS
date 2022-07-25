#pragma once

#include "device.h"

#if defined(PLATFORM_ESP32)
extern device_t BLET_device;
void ICACHE_RAM_ATTR BluetoothTelemetrykUpdateValues(uint8_t *data);
#define HAS_BLET
#endif