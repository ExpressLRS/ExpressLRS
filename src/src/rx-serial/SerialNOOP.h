#pragma once

#include "SerialIO.h"

class SerialNOOP : public SerialIO {
public:
    explicit SerialNOOP() : SerialIO(nullptr, nullptr) {}

    virtual ~SerialNOOP() {}

    void setLinkQualityStats(uint16_t lq, uint16_t rssi) override {}
    void sendLinkStatisticsToFC() override {}
    void sendMSPFrameToFC(uint8_t* data) override {}

    uint32_t sendRCFrameToFC(bool frameAvailable, uint32_t *channelData) override { return  10; }

    void handleUARTin() {}
    void handleUARTout() { _fifo.flush(); }

private:
    void processByte(uint8_t byte) override {}
};
