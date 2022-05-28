#pragma once

#include <stdint.h>

void bitpacker_unpack(uint8_t const * const src, unsigned srcBits, uint32_t * const dest, unsigned dstBits, unsigned numOfChannels);
