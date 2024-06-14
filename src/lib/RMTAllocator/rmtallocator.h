//
// Authors:
// * Dominic Clifton
//

#pragma once

#if defined(PLATFORM_ESP32)

#include "driver/rmt.h"

/**
 * @brief Allocates RMT TX or RX channels from a pool
 *
 * Depending on the RMT periphal some channels may be TX or RX only or can do both.
 * The allocator only cares if a channel is allocated, and does not care what the channel is used for after it's allocated.
 */
class RMTAllocator {
public:
    RMTAllocator() {}

    /**
     * @brief Allocate a channel for transmitting
     * @param channel reference to a channel to update if the allocation succeeds.
     * @return true on successful allocation.
     */
    bool allocateTX(rmt_channel_t &channel);
    /**
     * @brief Allocate a channel for receiving
     *
     * @param channel reference to a channel to update if the allocation succeeds.
     * @return true on successful allocation.
     */
    bool allocateRX(rmt_channel_t &channel);

    /**
     * @brief Release a channel.
     *
     * @param the channel to release
     */
    void release(rmt_channel_t channel);

private:
    uint8_t allocationsMask;
};

extern RMTAllocator rmtAllocator;

#endif