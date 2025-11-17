#pragma once

#include "SerialIO.h"

class SerialNOOP final : public SerialIO {
public:
    explicit SerialNOOP() : SerialIO(nullptr, nullptr) {}
    ~SerialNOOP() override = default;

    uint32_t sendRCFrame(bool frameAvailable, bool frameMissed, uint32_t *channelData) override { return  DURATION_NEVER; }

    void processSerialInput() override {}

private:
    void processBytes(uint8_t *bytes, uint16_t size) override {}
};
