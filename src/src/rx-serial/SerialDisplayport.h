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

#pragma once
#include "SerialIO.h"

#define MSP_STATUS          101
#define MSP_STATUS_EX       150
#define MSP_MSG_PERIOD_MS   100

// MSP_STATUS_DJI
struct msp_status_DJI_t
{
  uint16_t cycleTime;
  uint16_t i2cErrorCounter;
  uint16_t sensor;                    // MSP_STATUS_SENSOR_...
  uint32_t flightModeFlags;           // see getActiveModes()
  uint8_t  configProfileIndex;
  uint16_t averageSystemLoadPercent;  // 0...100
  uint16_t armingFlags;   //0x0103 or 0x0301
  uint8_t  accCalibrationAxisFlags;  //0
  uint8_t  DJI_ARMING_DISABLE_FLAGS_COUNT; //25
  uint32_t djiPackArmingDisabledFlags; //(1 << 24)
} __attribute__ ((packed));

////////////////////////////

class SerialDisplayport : public SerialIO
{
public:
    explicit SerialDisplayport(Stream &out, Stream &in) : SerialIO(&out, &in), m_lastSentMSP(0) {}
    virtual ~SerialDisplayport() {}

    void queueLinkStatisticsPacket() override {}
    void queueMSPFrameTransmission(uint8_t* data) override {};
    void sendQueuedData(uint32_t maxBytesToSend) override {};
    uint32_t sendRCFrame(bool frameAvailable, bool frameMissed, uint32_t *channelData) override;

private:
    void processBytes(uint8_t *bytes, uint16_t size) override {};
    void send(uint8_t messageID, void * payload, uint8_t size, Stream * _stream);

    uint32_t m_lastSentMSP;
};
