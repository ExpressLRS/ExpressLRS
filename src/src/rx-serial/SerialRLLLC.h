#pragma once

#include "SerialIO.h"

class SerialRLLLC final : public SerialIO {
public:
    explicit SerialRLLLC(Stream &out, Stream &in) : SerialIO(&out, &in) {}
    ~SerialRLLLC() override = default;

    uint32_t sendRCFrame(bool frameAvailable, bool frameMissed, uint32_t *channelData) override;

private:
    void processBytes(uint8_t *bytes, uint16_t size) override {};
    static uint8_t mapCrsfToByte(uint32_t value);
};
