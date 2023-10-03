#include "SerialIO.h"
#include "FIFO.h"
#include "telemetry_protocol.h"

// Variables / constants
extern FIFO<AP_MAX_BUF_LEN> mavlinkInputBuffer;
extern FIFO<AP_MAX_BUF_LEN> mavlinkOutputBuffer;

class SerialMavlink : public SerialIO {
public:
    explicit SerialMavlink(Stream &out, Stream &in) : SerialIO(&out, &in) {lastSentFlowCtrl = 0;}
    virtual ~SerialMavlink() {}

    void queueLinkStatisticsPacket() override {}
    void queueMSPFrameTransmission(uint8_t* data) override {}
    uint32_t sendRCFrame(bool frameAvailable, uint32_t *channelData) override;

    int getMaxSerialReadSize() override;
    void sendQueuedData(uint32_t maxBytesToSend) override;

private:
    void processBytes(uint8_t *bytes, u_int16_t size) override;

    uint32_t lastSentFlowCtrl;
};
