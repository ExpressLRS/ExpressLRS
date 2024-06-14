//
// Authors:
// * Dominic Clifton
//

#if defined(PLATFORM_ESP32)

#include "targets.h"

#include "rmtallocator.h"

#if defined(PLATFORM_ESP32_S3)
// See esp32-s3_technical_reference_manual_en.pdf, section 37 Remote Control Peripheral (RMT), 37.1 Overview
// 0-3 are dedicated to TX
#define TX_MASK 0x0F
// 4-7 are dedicated to RX
#define RX_MASK 0xF0
#else
// See esp32_technical_reference_manual_en.pdf, section 15.2.1 RMT Architecture
#define TX_MASK 0xFF
#define RX_MASK 0xFF
#endif

uint8_t modeMasks[2] = {TX_MASK, RX_MASK};

enum eRRMTMode {
    MODE_TX = 0,
    MODE_RX,
};

bool allocate(eRRMTMode mode, rmt_channel_t &channel, uint8_t &allocationMask) {
    uint8_t modeMask = modeMasks[mode];

    for (uint8_t candidate = 0; candidate < SOC_RMT_CHANNELS_PER_GROUP; candidate++) {
        uint8_t candidateMask = 1 << candidate;

        if ((candidateMask & modeMask) == 0) {
            // channel cannot be used for this mode
            continue;
        }

        if ((candidateMask & allocationMask) == 0) {
            channel = (rmt_channel_t)((uint8_t)RMT_CHANNEL_0 + candidate);
            allocationMask |= candidateMask;
            return true;
        }
    }
    return false;
}

bool RMTAllocator::allocateTX(rmt_channel_t &channel) {
    return allocate(MODE_TX, channel, allocationsMask);
}

bool RMTAllocator::allocateRX(rmt_channel_t &channel) {
    return allocate(MODE_RX, channel, allocationsMask);
}

// no effect if not allocated
void RMTAllocator::release(rmt_channel_t channel) {
    uint8_t index = (uint8_t)RMT_CHANNEL_0 + (uint8_t)channel;
    allocationsMask &= ~(1 << index);
}

RMTAllocator rmtAllocator;

#endif