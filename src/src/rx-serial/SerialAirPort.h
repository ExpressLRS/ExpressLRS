#pragma once

#include "OTA.h"
#include "SerialIO.h"
#include "FIFO.h"

class SerialAirPort final : public SerialIO {
public:
    explicit SerialAirPort(Stream &out, Stream &in) : SerialIO(&out, &in) {}
    ~SerialAirPort() override = default;

    uint32_t sendRCFrame(bool frameAvailable, bool frameMissed, uint32_t *channelData) override;

    int getMaxSerialReadSize() override;
    void sendQueuedData(uint32_t maxBytesToSend) override;

    FIFO<AP_MAX_BUF_LEN> apInputBuffer;
    FIFO<AP_MAX_BUF_LEN> apOutputBuffer;

    bool isTlmQueued() const { return apInputBuffer.size() > 0; }
private:
    void processBytes(uint8_t *bytes, u_int16_t size) override;
};
