#pragma once

#if defined(TARGET_TX) && defined(PLATFORM_ESP32)

#include <WiFiUdp.h>
#include <AsyncUDP.h>

//#define HAS_WIFI_JOYSTICK 1
#define JOYSTICK_PORT 11000
#define JOYSTICK_DEFAULT_UPDATE_INTERVAL 10000
#define JOYSTICK_DEFAULT_CHANNEL_COUNT 8

class WifiJoystick
{
public:
    static void StartJoystickService();
    static void StopJoystickService();
    static void UpdateValues();
    static void StartSending(IPAddress ip, uint32_t updateInterval, uint8_t newChannelCount);
    static void Loop(unsigned long now);
private:
    static WiFiUDP *udp;
    static IPAddress remoteIP;
    static uint8_t channelCount;
      
};

#endif
