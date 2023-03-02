#pragma once

#include "targets.h"

class EspFlashStream : public Stream
{
public:
    EspFlashStream();
    // Set the starting address to use with seek()s
    void setBaseAddress(size_t base);
    size_t getPosition() const { return _flashOffset + _bufferPos; }
    void setPosition(size_t offset);

    // Stream class overrides
    virtual size_t write(uint8_t) { return 0; }
    virtual int available() { return (_bufferPos <= sizeof(_buffer)) ? 1 : 0; }
    virtual int read();
    virtual int peek();

private:
    WORD_ALIGNED_ATTR uint8_t _buffer[4];
    size_t _flashBase;
    size_t _flashOffset;
    uint8_t _bufferPos;

    void fillBuffer();
};