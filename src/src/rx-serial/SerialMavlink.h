#pragma once

#include "SerialIO.h"
#include "FIFO.h"

#define MAV_INPUT_BUF_LEN       1024
#define MAV_OUTPUT_BUF_LEN      512
#define MAV_PAYLOAD_SIZE_MAX    60

// Variables / constants
extern FIFO<MAV_INPUT_BUF_LEN> mavlinkInputBuffer;
extern FIFO<MAV_OUTPUT_BUF_LEN> mavlinkOutputBuffer;

class SerialMavlink final : public SerialIO {
public:
    explicit SerialMavlink(Stream &out, Stream &in);
    ~SerialMavlink() override = default;

    void queueLinkStatisticsPacket() override {}
    uint32_t sendRCFrame(bool frameAvailable, bool frameMissed, uint32_t *channelData) override;

    int getMaxSerialReadSize() override;
    void sendQueuedData(uint32_t maxBytesToSend) override;

private:
    void processBytes(uint8_t *bytes, u_int16_t size) override;

    const uint8_t this_system_id;
    const uint8_t this_component_id;

    const uint8_t target_system_id;
    const uint8_t target_component_id;

    uint32_t lastSentFlowCtrl = 0;
};
