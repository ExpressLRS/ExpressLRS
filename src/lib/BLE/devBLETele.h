#pragma once

#include "device.h"

#if defined(PLATFORM_ESP32)
extern device_t BLET_device;
void ICACHE_RAM_ATTR BluetoothTelemetryUpdateValues(uint8_t *data);
void ICACHE_RAM_ATTR BluetoothTelemetrySendLinkStatsPacket();
void ICACHE_RAM_ATTR BluetoothTelemetrySendEmptyLinkStatsPacket();
void ICACHE_RAM_ATTR BluetoothTelemetrySendRCFrame();
#define HAS_BLET
#endif