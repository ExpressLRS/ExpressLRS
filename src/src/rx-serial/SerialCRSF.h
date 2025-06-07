#pragma once
#include "SerialIO.h"

#include "CRSFParser.h"
#include "CRSFRouter.h"

class SerialCRSF final : public SerialIO, public CRSFConnector {
public:
    explicit SerialCRSF(Stream &out, Stream &in)
        : SerialIO(&out, &in)
    {
        crsfRouter.addConnector(this);
    }
    ~SerialCRSF() override
    {
        crsfRouter.removeConnector(this);
    }

    uint32_t sendRCFrame(bool frameAvailable, bool frameMissed, uint32_t *channelData) override;
    void forwardMessage(const crsf_header_t *message) override;

    bool sendImmediateRC() override { return true; }

private:
    CRSFParser crsfParser;

    void processBytes(uint8_t *bytes, uint16_t size) override;
};
