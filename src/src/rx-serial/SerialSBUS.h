#include "SerialIO.h"

class SerialSBUS : public SerialIO {
public:
    explicit SerialSBUS(Stream &out, Stream &in) : SerialIO(&out, &in)
    {
        streamOut = &out;
    }

    ~SerialSBUS() override = default;
    
    eSerialProtocolType getProtocol(){return SERIAL_PROTOCOL_SBUS;};

    void queueLinkStatisticsPacket() override {}
    void queueMSPFrameTransmission(uint8_t* data) override {}
    uint32_t sendRCFrame(bool frameAvailable, bool frameMissed, uint32_t *channelData) override;

private:
    void processBytes(uint8_t *bytes, uint16_t size) override {};

    Stream *streamOut;
};
