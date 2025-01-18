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

#pragma once
#include "SerialIO.h"

#define MSP_STATUS          101
#define MSP_STATUS_EX       150
#define MSP_MSG_PERIOD_MS   100

struct msp_status_t
{
    uint16_t task_delta_time;
    uint16_t i2c_error_count;
    uint16_t sensor_status;
    uint32_t flight_mode_flags;
    uint8_t pid_profile;
    uint16_t system_load;
    uint16_t gyro_cycle_time;
    uint8_t box_mode_flags;
    uint8_t arming_disable_flags_count;
    uint32_t arming_disable_flags;
    uint8_t extra_flags;
} __attribute__ ((packed));

////////////////////////////

class SerialDisplayport : public SerialIO
{
public:
    explicit SerialDisplayport(Stream &out, Stream &in) : SerialIO(&out, &in), m_receivedBytes(0), m_receivedTimestamp(0) {}
    virtual ~SerialDisplayport() {}

    void queueLinkStatisticsPacket() override {}
    void queueMSPFrameTransmission(uint8_t* data) override {};
    void sendQueuedData(uint32_t maxBytesToSend) override {};
    uint32_t sendRCFrame(bool frameAvailable, bool frameMissed, uint32_t *channelData) override;

private:
    void processBytes(uint8_t *bytes, uint16_t size) override;
    void send(uint8_t messageID, void * payload, uint8_t size, Stream * _stream);
    bool getArmedState();

    uint8_t m_receivedBytes;
    uint32_t m_receivedTimestamp;
};

#endif // defined(TARGET_RX)
