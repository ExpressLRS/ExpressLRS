#pragma once

#include "SerialIO.h"
#include "FIFO.h"
#include "telemetry_protocol.h"

// Variables / constants for Airport //
extern FIFO<AP_MAX_BUF_LEN> apInputBuffer;
extern FIFO<AP_MAX_BUF_LEN> apOutputBuffer;

class SerialAirPort final : public SerialIO {
public:
    explicit SerialAirPort(Stream &out, Stream &in) : SerialIO(&out, &in) {}
    ~SerialAirPort() override = default;

    void queueLinkStatisticsPacket() override {}
    uint32_t sendRCFrame(bool frameAvailable, bool frameMissed, uint32_t *channelData) override;

    int getMaxSerialReadSize() override;
    void sendQueuedData(uint32_t maxBytesToSend) override;

private:
    void processBytes(uint8_t *bytes, u_int16_t size) override;
};
