/************************************************************************************
Credits:
  This software is based on and uses software published by Richard Amiss 2023,
  QLiteOSD, which is based on work by Paul Kurucz (pkuruz):opentelem_to_bst_bridge 
  as well as software from d3ngit : djihdfpv_mavlink_to_msp_V2 and 
  crashsalot : VOT_to_DJIFPV

THIS SOFTWARE IS PROVIDED IN AN "AS IS" CONDITION. NO WARRANTIES, WHETHER EXPRESS, 
IMPLIED OR STATUTORY, INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF 
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE APPLY TO THIS SOFTWARE. THE 
COMPANY SHALL NOT, IN ANY CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL OR 
CONSEQUENTIAL DAMAGES, FOR ANY REASON WHATSOEVER.
************************************************************************************/

#if defined(TARGET_RX)

#include "SerialDisplayport.h"
#include "crsf_protocol.h"
#include "OTA.h"

void SerialDisplayport::send(uint8_t messageID, void * payload, uint8_t size, Stream * _stream)
{
  _stream->write('$');
  _stream->write('M');
  _stream->write('<');
  _stream->write(size);
  _stream->write(messageID);
  uint8_t checksum = size ^ messageID;
  uint8_t * payloadPtr = (uint8_t*)payload;
  for (uint8_t i = 0; i < size; ++i)
  {
    uint8_t b = *(payloadPtr++);
    checksum ^= b;
  }
  _stream->write((uint8_t*)payload, size);
  _stream->write(checksum);
}

uint32_t SerialDisplayport::sendRCFrame(bool frameAvailable, bool frameMissed, uint32_t *channelData)
{
    // Send extended status MSP
    msp_status_DJI_t status_DJI;
    status_DJI.cycleTime = 0x0080;
    status_DJI.i2cErrorCounter = 0;
    status_DJI.sensor = 0x23;
    status_DJI.flightModeFlags = isArmed ? 0x3 : 0x2;
    status_DJI.configProfileIndex = 0;
    status_DJI.averageSystemLoadPercent = 7;
    status_DJI.accCalibrationAxisFlags = 0;
    status_DJI.DJI_ARMING_DISABLE_FLAGS_COUNT = 20;
    status_DJI.djiPackArmingDisabledFlags = (1 << 24);
    status_DJI.armingFlags = 0x0303;
    send(MSP_STATUS_EX, &status_DJI, sizeof(status_DJI), _outputPort);

    // Send status MSP
    status_DJI.armingFlags = 0x0000;
    send(MSP_STATUS, &status_DJI, sizeof(status_DJI), _outputPort);

    return MSP_MSG_PERIOD_MS;   // Send MSP msgs to DJI at 10Hz
}

#endif