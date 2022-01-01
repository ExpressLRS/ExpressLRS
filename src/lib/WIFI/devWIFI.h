#pragma once

#include "device.h"

#if defined(PLATFORM_ESP32) || defined(PLATFORM_ESP8266)
extern device_t WIFI_device;
void WSnotifyAll(const char *msg, int len);
void MSP2WIFI(const char *msg, uint32_t len);
void MSP2WIFIhandleDelayed();
#define HAS_WIFI
#endif