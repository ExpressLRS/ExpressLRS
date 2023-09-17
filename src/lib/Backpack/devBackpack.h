#pragma once

#include "device.h"

void checkBackpackUpdate();
void crsfTelemToMSPOut(uint8_t *data);
extern bool HTEnableFlagReadyToSend;

extern device_t Backpack_device;
