#pragma once

#include "device.h"
#include "crsf_protocol.h"

// RF ISR publication and serial delivery share the same generation/latches.
struct SerialRCFrameState
{
    volatile uint32_t generation = 0, frameGeneration = 0, deliveredGeneration = 0;
    volatile bool available = false, missed = false;
    __attribute__((always_inline)) void invalidate()
    {
        available = missed = false;
        ++generation;
    }
    __attribute__((always_inline)) bool takeAvailable()
    {
        const bool result = available;
        deliveredGeneration = frameGeneration;
        available = false;
        return result && deliveredGeneration == generation;
    }
};
extern SerialRCFrameState serialRCFrames[2];
extern void crsfRCFrameReset();

inline uint32_t serialChannelValue(uint32_t value, bool preserveUnset)
{
    return value == CRSF_CHANNEL_VALUE_UNSET && !preserveUnset ? CRSF_CHANNEL_VALUE_EXT_MIN : value;
}
