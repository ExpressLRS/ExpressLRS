#pragma once

#include "device.h"

extern device_t Serial0_device;
#if defined(PLATFORM_ESP32)
extern device_t Serial1_device;
#endif
extern void sendImmediateRC(uint32_t ReceivedChannelData[CRSF_NUM_CHANNELS]);
extern void handleSerialIO();
extern void crsfRCFrameAvailable();
extern void crsfRCFrameMissed();
