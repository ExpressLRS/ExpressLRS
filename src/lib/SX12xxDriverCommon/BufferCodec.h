#pragma once

#include <stdint.h>

// How the LR11xx/LR20xx drivers pack an OTA packet into the radio's buffer (plain copy, or FEC)
class BufferCodec
{
public:
    virtual ~BufferCodec() {}
    virtual void encode(uint8_t *out, uint8_t *in, uint32_t len);
    virtual void decode(uint8_t *out, uint8_t *in, uint32_t len);
};
