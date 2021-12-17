#pragma once

#include "device.h"

#if defined(PLATFORM_ESP32) || defined(PLATFORM_ESP8266)
extern device_t WIFI_device;
void WSnotifyAll(const char *msg, int len);
bool wsConnected;
#define HAS_WIFI
#endif