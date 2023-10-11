#pragma once

#if defined(TARGET_TX) && defined(PLATFORM_ESP32)

#include <WiFiUdp.h>

#define HAS_WIFI_JOYSTICK 1
#define JOYSTICK_PORT 11000
#define JOYSTICK_DEFAULT_UPDATE_INTERVAL 10000
#define JOYSTICK_DEFAULT_CHANNEL_COUNT 8
#define JOYSTICK_VERSION 1

/**
 * Class to send stick data via udp
 * Version 1
 *
 * Usage for simulator or driver on PC:
 *
 * Step 1: Find device. There are two options:
 *   Recommended: Use MDNS to query for a UDP service called elrs to find out IP+Port, Protocol version can be found in service txt version
 *   Alternative: Listen for UDP broadcasts that contains a frame in the structure of
 *     4 bytes: ['E', 'L', 'R', 'S']
 *     1 byte: Protocol Version
 *     2 byte unsigned: PORT, little-endian
 *     1 byte: length of Device Name
 *     ASCII Text: Device Name
 *
 * Step 2:
 *   Start udp socket recvfrom(IP, PORT) discovered in Step 1
 *
 * Step 3:
 *   Send HTTP POST request to device URL http://<IP>/udpcontrol
 *   Param: "action" must be "joystick_begin"
 *   Param (optional): "interval" in us to send updates, or 0 for default (10ms)
 *   Param (optional): "channels" number of channels to send in each frame, or 0 for default (8)
 *   e.g. http://<IP>/udpcontrol?action=joystick_begin&interval=10000&channels=8
 *
 * Step 3:
 *   receive frames in the format of:
 *   1 byte: Frame type (WifiJoystickFrameType_e)CHANNELS
 *   1 byte: Number of channels that follow
 *   2 bytes unsigned * channel count: Channel data in range 0 to 0xffff, little-endian
 *
 * Step 4:
 *  To end joystick data being sent, POST to the control URL
 *  Param: "action" must be "joystick_end"
 *  e.g http://<IP>/udpcontrol?action=joystick_end
 */

class WifiJoystick
{
public:
    enum WifiJoystickFrameType_e {
        FRAME_CHANNELS = 1
    };
    static void StartJoystickService();
    static void StopJoystickService();
    static void UpdateValues();
    static void StartSending(const IPAddress& ip, int32_t updateInterval, uint8_t newChannelCount);
    static void StopSending() { active = false; }
    static void Loop(unsigned long now);
private:
    static WiFiUDP *udp;
    static IPAddress remoteIP;
    static uint8_t channelCount;
    static bool active;
};

#endif
