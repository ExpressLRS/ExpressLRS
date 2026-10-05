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
