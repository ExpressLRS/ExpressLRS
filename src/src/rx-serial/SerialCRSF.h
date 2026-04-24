#include "SerialIO.h"

class SerialCRSF : public SerialIO {
public:
    explicit SerialCRSF(Stream &out, Stream &in) : SerialIO(&out, &in) {}

    virtual ~SerialCRSF() {}

    void setLinkQualityStats(uint16_t lq, uint16_t rssi) override;
    uint32_t sendRCFrameToFC(bool frameAvailable, uint32_t *channelData) override;
    void sendMSPFrameToFC(uint8_t* data) override;
    void sendLinkStatisticsToFC() override;

private:
    uint16_t linkQuality = 0;
    uint16_t rssiDBM = 0;

    void processByte(uint8_t byte) override;
};
