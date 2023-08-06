#include "SerialIO.h"
#include "FIFO_GENERIC.h"
#include "telemetry_protocol.h"

// Variables / constants
#define MAX_MAVLINK_BUF_LEN  2048
extern FIFO_GENERIC<AP_MAX_BUF_LEN> mavlinkInputBuffer;
extern FIFO_GENERIC<AP_MAX_BUF_LEN> mavlinkOutputBuffer;

class SerialMavlink : public SerialIO {
public:
    explicit SerialMavlink(Stream &out, Stream &in) : SerialIO(&out, &in) {lastSentFlowCtrl = 0;}

    virtual ~SerialMavlink() {}

    void setLinkQualityStats(uint16_t lq, uint16_t rssi) override;
    uint32_t sendRCFrameToFC(bool frameAvailable, uint32_t *channelData) override;
    void sendMSPFrameToFC(uint8_t* data) override;
    void sendLinkStatisticsToFC() override;

    int getMaxSerialReadSize() override;
    void handleUARTout() override;

private:
    void processBytes(uint8_t *bytes, u_int16_t size) override;
    void processByte(uint8_t byte) override {};

    uint32_t lastSentFlowCtrl;
};
