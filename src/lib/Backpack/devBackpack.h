#pragma once

#include "device.h"

#if defined(PLATFORM_ESP32)
void checkBackpackUpdate();
void sendCRSFTelemetryToBackpack(uint8_t *data);
void sendMAVLinkTelemetryToBackpack(uint8_t *data);
#else
void checkBackpackUpdate() {}
void sendCRSFTelemetryToBackpack(uint8_t *data) {}
void sendMAVLinkTelemetryToBackpack(uint8_t *data) {}
#endif

extern bool HTEnableFlagReadyToSend;
extern bool BackpackTelemReadyToSend;

extern device_t Backpack_device;
