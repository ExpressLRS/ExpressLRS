#pragma once

#include "SerialIO.h"
#include "msp_protocol.h"
#include "msp_utils.h"
#include "CRSFRouter.h"

class SerialMSP : public SerialIO {
public:
    explicit SerialMSP(Stream &out, Stream &in) : SerialIO(&out, &in) {
        MSPUtils::init();
        MSPUtils::parseReset(&linkInStatus);
        MSPUtils::parseReset(&serialInStatus);
        injectRcChannels = false;
        injectLinkStats = false;
        injectRcInfo = false;
        linkStatsDisabled = false;
        rcInfoDisabled = false;
        rcChannelsLastMs = 0;
        linkStatsLastMs = 0;
        rcInfoLastMs = 0;
    }
    virtual ~SerialMSP() {}

    uint32_t sendRCFrame(bool frameAvailable, bool frameMissed, uint32_t *channelData) override;
    void queueMSPFrameTransmission(uint8_t* data);
    void queueLinkStatisticsPacket();
    void sendQueuedData(uint32_t maxBytesToSend) override;

    bool sendImmediateRC() override { return false; }

private:
    void processBytes(uint8_t *bytes, uint16_t size) override;
    
    void sendRcChannels(uint32_t *channelData);
    void sendLinkStats();
    void sendRcInfo();
    
    msp_parse_status_t linkInStatus;
    msp_message_t linkInMsg;
    
    msp_parse_status_t serialInStatus;
    msp_message_t serialInMsg;
    
    tMspSetRawRc rcChannels;
    bool injectRcChannels;
    bool injectLinkStats;
    bool injectRcInfo;
    bool linkStatsDisabled;
    bool rcInfoDisabled;
    
    uint32_t rcChannelsLastMs;
    uint32_t linkStatsLastMs;
    uint32_t rcInfoLastMs;
    
    uint8_t workBuffer[MSP_FRAME_LEN_MAX];
};