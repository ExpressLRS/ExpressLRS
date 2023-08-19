#include "SerialIO.h"
#include "crc.h"

class SerialSUMD : public SerialIO {
public:
    explicit SerialSUMD(Stream &out, Stream &in) : SerialIO(&out, &in) { crc2Byte.init(16, 0x1021); }
    virtual ~SerialSUMD() {}

    void sendLinkStatisticsToFC() override {}
    void sendMSPFrameToFC(uint8_t* data) override {}
    uint32_t sendRCFrameToFC(bool frameAvailable, uint32_t *channelData) override;

private:
    Crc2Byte crc2Byte;
    void processBytes(uint8_t *bytes, uint16_t size) override {};
};
