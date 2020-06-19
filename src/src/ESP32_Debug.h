#pragma once 

#include "SX127xDriver.h"

extern SX127xDriver Radio;

void DEBUG_PrintRadioPacketStats()
{
    Serial.print("SPItime: ");
    Serial.println(Radio.TXspiTime);
    Serial.print("TotalTime: ");
    Serial.println(Radio.TimeOnAir);
    Serial.print("HeadRoom: ");
    Serial.println(Radio.HeadRoom);
    Serial.print("PacketCount(HZ): ");
    Serial.println(Radio.PacketCount);
}