#pragma once 

#include "LoRaRadioLib.h"

extern SX127xDriver Radio;

void DEBUG_PrintRadioPacketStats()
{
    DEBUG_PRINT("SPItime: ");
    DEBUG_PRINTLN(Radio.TXspiTime);
    DEBUG_PRINT("TotalTime: ");
    DEBUG_PRINTLN(Radio.TimeOnAir);
    DEBUG_PRINT("HeadRoom: ");
    DEBUG_PRINTLN(Radio.HeadRoom);
    DEBUG_PRINT("PacketCount(HZ): ");
    DEBUG_PRINTLN(Radio.PacketCount);
}