#include "targets.h"

void ICACHE_RAM_ATTR bitpacker_unpack(uint8_t const * const src, unsigned srcBits, uint32_t * const dest, unsigned dstBits, unsigned numOfChannels)
{
    unsigned const channelMask = (1 << srcBits) - 1;
    unsigned const precisionShift = dstBits - srcBits;

    // code from BetaFlight rx/crsf.cpp
    uint8_t bitsMerged = 0;
    uint32_t readValue = 0;
    unsigned readByteIndex = 0;
    for (unsigned n = 0; n < numOfChannels; n++)
    {
        while (bitsMerged < srcBits)
        {
            uint8_t readByte = src[readByteIndex++];
            readValue |= ((uint32_t) readByte) << bitsMerged;
            bitsMerged += 8;
        }
        //printf("rv=%x(%x) bm=%u\n", readValue, (readValue & channelMask), bitsMerged);
        dest[n] = (readValue & channelMask) << precisionShift;
        readValue >>= srcBits;
        bitsMerged -= srcBits;
    }
}