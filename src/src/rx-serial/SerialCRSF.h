#include "SerialIO.h"

class SerialCRSF : public SerialIO {
public:
    explicit SerialCRSF(Stream &out, Stream &in) : SerialIO(&out, &in) {}

    virtual ~SerialCRSF() {}

    uint32_t sendRCFrameToFC(bool frameAvailable, uint32_t *channelData) override;
    void sendMSPFrameToFC(uint8_t* data) override;
    void sendLinkStatisticsToFC() override;
    void handleUARTout() override;

private:
    static const uint32_t SERIAL_OUTPUT_FIFO_SIZE = 256U;
    FIFO<SERIAL_OUTPUT_FIFO_SIZE> _fifo;

    void processByte(uint8_t byte) override;
};
