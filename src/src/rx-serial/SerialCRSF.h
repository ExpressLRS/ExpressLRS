#pragma once
#include "SerialIO.h"

#include "CRSFConnector.h"
#include "CRSFEndpoint.h"

class SerialCRSF final : public SerialIO, public CRSFConnector {
public:
    explicit SerialCRSF(Stream &out, Stream &in)
        : SerialIO(&out, &in)
    {
        crsfEndpoint->addConnector(this);
    }
    ~SerialCRSF() override = default;

    uint32_t sendRCFrame(bool frameAvailable, bool frameMissed, uint32_t *channelData) override;
    void forwardMessage(const crsf_header_t *message) override;
    void sendQueuedData(uint32_t maxBytesToSend) override;

    bool sendImmediateRC() override { return true; }

private:
    void processBytes(uint8_t *bytes, uint16_t size) override;
};
