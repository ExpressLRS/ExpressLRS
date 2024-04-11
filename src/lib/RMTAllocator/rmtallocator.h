//
// Authors:
// * Dominic Clifton
//

#pragma once

#if defined(PLATFORM_ESP32)

#include "driver/rmt.h"

/**
 * Allocates TX or RX channels from a pool, depending on the RMT periphal some channels may be TX or RX only or can do both.
 * The allocator only cares if a channel is allocated, and does not care what the channel is used for after it's allocated.
*/
class RMTAllocator {
public:
    RMTAllocator() {}

    bool allocateTX(rmt_channel_t &channel);
    bool allocateRX(rmt_channel_t &channel);
    void release(rmt_channel_t channel);

private:
    uint8_t allocationsMask;
};

extern RMTAllocator rmtAllocator;

#endif