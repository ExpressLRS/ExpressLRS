#pragma once
#include "../test_msp/mock_serial.h"

// The upstream string mock uses signed chars; this adapter needs binary bytes.
class BinaryStringStream : public StringStream
{
public:
    using StringStream::StringStream;
    int read() override { return available() ? static_cast<uint8_t>(StringStream::read()) : -1; }
    int peek() override { return available() ? static_cast<uint8_t>(StringStream::peek()) : -1; }
};

// Record the hardware UART boundary while the actual SRXL2 constructor runs.
struct UartStartupSpy
{
    uint32_t baud = 0, format = 0;
    int8_t rxPin = -1, txPin = -1;
    bool inverted = false;
    bool beginSucceeds = true, initialized = false;
    size_t txBufferSize = 1;
    uint8_t rxThreshold = 0;
    unsigned begins = 0, ends = 0;
    void setTxBufferSize(size_t size) { txBufferSize = size; }
    void begin(uint32_t rate, uint32_t config, int8_t rx, int8_t tx, bool invert)
    {
        ++begins;
        initialized = beginSucceeds;
        baud = rate; format = config; rxPin = rx; txPin = tx; inverted = invert;
    }
    void setRxFIFOFull(uint8_t threshold) { rxThreshold = threshold; }
    operator bool() const { return initialized; }
    void end() { ++ends; initialized = false; }
};
