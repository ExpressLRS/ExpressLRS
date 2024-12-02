#pragma once

#include "device.h"

void checkBackpackUpdate();
void sendCRSFTelemetryToBackpack(uint8_t *data);
void sendMAVLinkTelemetryToBackpack(uint8_t *data);

extern bool HTEnableFlagReadyToSend;
extern bool BackpackTelemReadyToSend;

extern device_t Backpack_device;
