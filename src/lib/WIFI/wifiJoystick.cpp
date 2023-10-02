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
   static unsigned long millis = 0;
   if(!udp || active)
   {
       return;
   }

   // broadcast wifi joystick options
   if(now >= millis + 5000)
   {
      // DBGLN("SSID: %s", WiFi.SSID());

       const char header[] = "ELRS_JOYSTICK";

       uint8_t lower = (JOYSTICK_PORT) & 0xff;
       uint8_t higher = (JOYSTICK_PORT >> (8*1)) & 0xff;

       udp->beginPacket("255.255.255.255", JOYSTICK_PORT);
       udp->write((uint8_t*)header, strlen(header));
       udp->write(JOYSTICK_VERSION);
       udp->write(higher);
       udp->write(lower);
       udp->write((uint8_t*)device_name, strlen(device_name));
       udp->endPacket();

       millis = now;
   }

   udp->flush();

}

/*
Frame format:
1 byte frame type
1 byte channel count
channel count * 2 bytes channel data in range 0 to 32767
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
        uint16_t channel = map(ChannelData[i], CRSF_CHANNEL_VALUE_MIN, CRSF_CHANNEL_VALUE_MAX, 0, 32767);
        udp->write((uint8_t*)&channel, 2);
    }
    udp->endPacket();
}

#endif
