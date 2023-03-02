#if defined(TARGET_UNIFIED_TX) || defined(TARGET_UNIFIED_RX)

#include "EspFlashStream.h"

EspFlashStream::EspFlashStream()
{
    setBaseAddress(0);
}

void EspFlashStream::fillBuffer()
{
    // Could also use spi_flash_read() here, but the return values differ between ESP (SPI_FLASH_RESULT_OK) and ESP32 (ESP_OK)
    if (ESP.flashRead(_flashBase + _flashOffset, (uint32_t *)_buffer, sizeof(_buffer)))
    {
        _bufferPos = 0;
    }
    else
    {
        // _bufferPos > sizeof() indicates error
        _bufferPos = sizeof(_buffer) + 1;
    }
}

void EspFlashStream::setBaseAddress(size_t base)
{
    _flashBase = base;
    _flashOffset = 0 - sizeof(_buffer); // underflow intentionally so offset+pos = 0
    _bufferPos = sizeof(_buffer);
}

void EspFlashStream::setPosition(size_t offset)
{
    // align(4), rounding down
    _flashOffset = (offset >> 2) << 2;
    // capture the new bufferPos before fill advances it
    uint8_t newBufferPos = offset - _flashOffset;

    fillBuffer();
    _bufferPos = newBufferPos;
}

int EspFlashStream::read()
{
    int retVal = peek();
    ++_bufferPos;
    return retVal;
}

int EspFlashStream::peek()
{
    if (_bufferPos == sizeof(_buffer))
    {
        _flashOffset += sizeof(_buffer);
        fillBuffer();
    }
    return _buffer[_bufferPos];
}

#endif