#include "SerialIO.h"
#include "FIFO_GENERIC.h"
#include "telemetry_protocol.h"

// Variables / constants for Airport //
extern FIFO_GENERIC<AP_MAX_BUF_LEN> apInputBuffer;
extern FIFO_GENERIC<AP_MAX_BUF_LEN> apOutputBuffer;

class SerialAirPort : public SerialIO {
public:
    explicit SerialAirPort(Stream &out, Stream &in) : SerialIO(&out, &in) {}

    virtual ~SerialAirPort() {}

    void setLinkQualityStats(uint16_t lq, uint16_t rssi) override;
    uint32_t sendRCFrameToFC(bool frameAvailable, uint32_t *channelData) override;
    void sendMSPFrameToFC(uint8_t* data) override;
    void sendLinkStatisticsToFC() override;

    int getMaxSerialReadSize() override;
    void handleUARTout() override;

private:
    void processBytes(uint8_t *bytes, u_int16_t size) override;
    void processByte(uint8_t byte) override {};
};
