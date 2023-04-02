#include "SerialIO.h"

class SerialSUMD : public SerialIO {
public:
    explicit SerialSUMD(Stream &out, Stream &in) : SerialIO(&out, &in) {}

    virtual ~SerialSUMD() {}

    void setLinkQualityStats(uint16_t lq, uint16_t rssi) override;
    uint32_t sendRCFrameToFC(bool frameAvailable, uint32_t *channelData) override;
    void sendMSPFrameToFC(uint8_t* data) override;
    void sendLinkStatisticsToFC() override;

private:
    uint16_t linkQuality = 0;
    uint16_t rssiDBM = 0;

    uint16_t crc16(uint8_t *data, uint8_t len);
    void processByte(uint8_t byte) override {};
};
