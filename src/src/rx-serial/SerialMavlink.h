#include "SerialIO.h"
#include "FIFO.h"
#include "telemetry_protocol.h"

#define MAV_INPUT_BUF_LEN   1024
#define MAV_OUTPUT_BUF_LEN  512
#ifndef RX_SYSID
#define RX_SYSID 1
#endif

#ifndef TX_SYSID
#define TX_SYSID 255
#endif


// Variables / constants
extern FIFO<MAV_INPUT_BUF_LEN> mavlinkInputBuffer;
extern FIFO<MAV_OUTPUT_BUF_LEN> mavlinkOutputBuffer;

class SerialMavlink : public SerialIO {
public:
    explicit SerialMavlink(Stream &out, Stream &in);
    virtual ~SerialMavlink() {}

    void queueLinkStatisticsPacket() override {}
    void queueMSPFrameTransmission(uint8_t* data) override {}
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
