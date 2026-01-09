#pragma once

#include <array>

#include "SerialIO.h"
#include "crc.h"

class SerialSUMD final : public SerialIO {
public:
    explicit SerialSUMD(Stream &out, Stream &in) : SerialIO(&out, &in) { crc2Byte.init(16, 0x1021); }
    ~SerialSUMD() override = default;

    uint32_t sendRCFrame(bool frameAvailable, bool frameMissed, uint32_t *channelData) override;

private:
    Crc2Byte crc2Byte {};
    void processBytes(uint8_t *bytes, uint16_t size) override {};
};

#if defined(WMEXTENSION)
class SerialSUMD3 : public SerialIO {
public:
    explicit SerialSUMD3(Stream &out, Stream &in) : SerialIO(&out, &in) { crc2Byte.init(16, 0x1021); }
    virtual ~SerialSUMD3() {}

    uint32_t sendRCFrame(bool frameAvailable, bool frameMissed, uint32_t* channelData) override;
private:
    Crc2Byte crc2Byte;
    void processBytes(uint8_t *bytes, uint16_t size) override {};

    enum class State : uint8_t {CH1_16, CH1_8_17_24, CH1_8_25_32, CH1_12_SW};

    void composeFrame(const uint8_t, const uint32_t* const channels, uint8_t* data);

    State state = State::CH1_16;
};
#endif