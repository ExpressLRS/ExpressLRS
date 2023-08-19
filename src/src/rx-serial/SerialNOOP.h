#pragma once

#include "SerialIO.h"

class SerialNOOP : public SerialIO {
public:
    explicit SerialNOOP() : SerialIO(nullptr, nullptr) {}
    virtual ~SerialNOOP() {}

    void sendLinkStatisticsToFC() override {}
    void sendMSPFrameToFC(uint8_t* data) override {}
    uint32_t sendRCFrameToFC(bool frameAvailable, uint32_t *channelData) override { return  DURATION_NEVER; }

    void handleUARTin() override {}

private:
    void processBytes(uint8_t *bytes, uint16_t size) override {}
};
