#include "device.h"

#include "wifiJoystick.h"

#if defined(HAS_WIFI_JOYSTICK)
#include <WiFi.h>
#include <WiFiUdp.h>
#include "common.h"
#include "CRSF.h"
#include "POWERMGNT.h"
#include "hwTimer.h"
#include "logging.h"


#if defined(Regulatory_Domain_AU_915) || defined(Regulatory_Domain_EU_868) || defined(Regulatory_Domain_IN_866) || defined(Regulatory_Domain_FCC_915) || defined(Regulatory_Domain_AU_433) || defined(Regulatory_Domain_EU_433)
extern SX127xDriver Radio;
#elif defined(Regulatory_Domain_ISM_2400)
extern SX1280Driver Radio;
#endif

WiFiUDP *WifiJoystick::udp = NULL;
IPAddress WifiJoystick::remoteIP;
uint8_t WifiJoystick::channelCount = JOYSTICK_DEFAULT_CHANNEL_COUNT;

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
    udp->stop();
    delete udp;
    udp = NULL;
    
}

void WifiJoystick::StartSending(IPAddress ip, uint32_t updateInterval, uint8_t newChannelCount)
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

    if (channelCount > 16) {
        channelCount = 16;
    }

}


void WifiJoystick::Loop(unsigned long now)
{
   static unsigned long millis = 0;
   if(!udp) 
   {
       return;
   } 

   if(now >= millis + 5000)
   {
      // DBGLN("SSID: %s", WiFi.SSID());  
       
       udp->beginPacket("255.255.255.255", 11000);
       udp->write((uint8_t*)"ELRS_WIFI", 9);
       udp->endPacket();
      
       millis = now;
   }
   
   udp->flush();

}

void WifiJoystick::UpdateValues()
{
    if (!udp)
    {
        return;
    }
    
    udp->beginPacket(remoteIP, JOYSTICK_PORT);
    for (uint8_t i = 0; i < channelCount; i++)
    {
        udp->write((uint8_t*)&ChannelData[i], 2); 
    }
    udp->endPacket();
    
}

#endif
