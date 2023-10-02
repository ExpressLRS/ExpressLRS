#pragma once

#if defined(TARGET_TX) && defined(PLATFORM_ESP32)

#include <WiFiUdp.h>
#include <AsyncUDP.h>

#define HAS_WIFI_JOYSTICK 1
#define JOYSTICK_PORT 11000
#define JOYSTICK_DEFAULT_UPDATE_INTERVAL 10000
#define JOYSTICK_DEFAULT_CHANNEL_COUNT 8
#define JOYSTICK_VERSION 1
#define JOYSTICK_VERSION_STRING "1"
#define JOYSTICK_CHANNEL_FRAME 1

/**
 * Class to send stick data via udp
 * Version 1
 *
 * Usage for simulator or driver on PC:
 *
 * Step 1: Find device. There are two options:
 *   Recommended: Use MDNS to query for a service called elrs-joystick to find out IP+Port, Protocol version can be found in service txt version
 *   Alternative: Listen for UDP broadcasts that contains a frame in the structure of
 *     13 bytes: ELRS_JOYSTICK
 *     1 byte: Protocol Version
 *     uint 2 bytes: PORT
 *     ASCII Text: Device Name
 *
 * Step 2:
 *   Start udp socket that listens on configured PORT
 *
 * Step 3:
 *   Send HTTP GET request to device in the form of http://<IP>/wifi_joystick?start=1&updateInterval=10000&channels=8
 *   updateInterval is in us
 *   channels defines how many channels are sent in each frame
 *
 * Step 3:
 *   receive frames in the format of:
 *   1 byte: Frame type = 1 for channel data
 *   1 byte: channel count
 *   <channel count>*2 bytes: channel data each channel 2 bytes in range 0 to 32767
 *
 */
class WifiJoystick
{
public:
    static void StartJoystickService();
    static void StopJoystickService();
    static void UpdateValues();
    static void StartSending(const IPAddress& ip, int32_t updateInterval, uint8_t newChannelCount);
    static void Loop(unsigned long now);
private:
    static WiFiUDP *udp;
    static IPAddress remoteIP;
    static uint8_t channelCount;
    static bool active;
};

#endif
