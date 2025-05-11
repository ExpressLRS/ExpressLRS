#pragma once

#include "SerialIO.h"
#include "CRSFRouter.h"
#include "device.h"

class SerialSmartAudio final : public SerialIO, public CRSFConnector {
public:
    explicit SerialSmartAudio(Stream &out, Stream &in, int8_t serial1TXpin);
    ~SerialSmartAudio() override = default;

    void sendQueuedData(uint32_t maxBytesToSend) override;
    uint32_t sendRCFrame(bool frameAvailable, bool frameMissed, uint32_t *channelData) override { return DURATION_IMMEDIATELY; }

    void forwardMessage(const crsf_header_t *message) override;

private:
    void processBytes(uint8_t *bytes, uint16_t size) override {};
    void setTXMode() const;
    void setRXMode() const;
#if defined(PLATFORM_ESP32)
    int8_t halfDuplexPin;
    uint8_t UTXDoutIdx;
    uint8_t URXDinIdx;
#endif
};
