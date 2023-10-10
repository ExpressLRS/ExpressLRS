#include "device.h"

#include "wifiJoystick.h"

#if defined(TARGET_TX) && defined(PLATFORM_ESP32)

#include <WiFi.h>
#include <WiFiUdp.h>
#include "common.h"
#include "CRSF.h"
#include "POWERMGNT.h"
#include "hwTimer.h"
#include "logging.h"
#include "options.h"


#if defined(RADIO_SX127X)
extern SX127xDriver Radio;
#elif defined(RADIO_SX128X)
extern SX1280Driver Radio;
#endif

WiFiUDP *WifiJoystick::udp = NULL;
IPAddress WifiJoystick::remoteIP;
uint8_t WifiJoystick::channelCount = JOYSTICK_DEFAULT_CHANNEL_COUNT;
bool WifiJoystick::active = false;

void WifiJoystick::StartJoystickService()
{
    if (!udp)
    {
        udp = new WiFiUDP();
        udp->begin(JOYSTICK_PORT);
    }
}

void WifiJoystick::StopJoystickService()
{
    active = false;
    if (udp)
    {
        udp->stop();
        delete udp;
        udp = NULL;
    }
}

void WifiJoystick::StartSending(const IPAddress& ip, int32_t updateInterval, uint8_t newChannelCount)
{
    if (!udp)
    {
        return;
    }
    remoteIP = ip;

    hwTimer::updateInterval(updateInterval);
    CRSF::setSyncParams(updateInterval);
    POWERMGNT::setPower(MinPower);
    Radio.End();

    CRSF::RCdataCallback = UpdateValues;
    channelCount = newChannelCount;

    if (channelCount > 16)
    {
        channelCount = 16;
    }
    active = true;
}


void WifiJoystick::Loop(unsigned long now)
{
    static unsigned long last = 0;
    if (!udp || active)
    {
       return;
    }

    // Advertise the existance of the server via broadcast
    constexpr unsigned BROADCAST_INTERVAL = 5000U;
    if (now - last > BROADCAST_INTERVAL)
    {
        last = now;

        struct ElrsUdpAdvertisement_s {
            uint32_t magic;     // ['E', 'L', 'R', 'S']
            uint8_t version;    // JOYSTICK_VERSION
            uint16_t port;      // JOYSTICK_PORT, network byte order
            uint8_t name_len;   // length of the device name that follows
            char name[0];       // device name
        } eua = {
            .magic = htobe32(0x454C5253),
            .version = JOYSTICK_VERSION,
            .port = htons(JOYSTICK_PORT),
            .name_len = (uint8_t)strlen(device_name)
        };

       udp->beginPacket(IPAddress(255, 255, 255, 255), JOYSTICK_PORT);
       udp->write((uint8_t*)&eua, sizeof(eua));
       udp->write((uint8_t*)device_name, eua.name_len);
       udp->endPacket();
    }

    // Free any received data
    udp->flush();
}

/*
Frame format:
1 byte frame type (JOYSTICK_CHANNEL_FRAME)
1 byte channel count
channel count * 2 bytes channel data in range 0 to 0xffff, network byte order
*/
void WifiJoystick::UpdateValues()
{
    if (!udp || !active)
    {
        return;
    }

    udp->beginPacket(remoteIP, JOYSTICK_PORT);
    udp->write(JOYSTICK_CHANNEL_FRAME);
    udp->write(channelCount);
    for (uint8_t i = 0; i < channelCount; i++)
    {
        uint16_t channel = htons(map(
            constrain(ChannelData[i], CRSF_CHANNEL_VALUE_MIN, CRSF_CHANNEL_VALUE_MAX),
            CRSF_CHANNEL_VALUE_MIN, CRSF_CHANNEL_VALUE_MAX, 0, 0xffff));
        udp->write((uint8_t*)&channel, 2);
    }
    udp->endPacket();
}

#endif
