#include "SerialIO.h"
#include "crc.h"

class SerialSUMD : public SerialIO {
public:
    explicit SerialSUMD(Stream &out, Stream &in) : SerialIO(&out, &in) { crc2Byte.init(16, 0x1021); }

    virtual ~SerialSUMD() {}

    Crc2Byte crc2Byte;

    void setLinkQualityStats(uint16_t lq, uint16_t rssi) override;
    uint32_t sendRCFrameToFC(bool frameAvailable, uint32_t *channelData) override;
    void sendMSPFrameToFC(uint8_t* data) override;
    void sendLinkStatisticsToFC() override;
    
    void sendByteToFC(uint8_t data) override;

private:
    uint16_t linkQuality = 0;
    uint16_t rssiDBM = 0;

    void processByte(uint8_t byte) override {};
};
