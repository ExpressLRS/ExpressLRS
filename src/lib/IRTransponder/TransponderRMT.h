//
// Authors: 
// * Mickey (mha1, initial RMT implementation)
// * Dominic Clifton (hydra, refactoring for multiple-transponder systems, iLap support)
//

#pragma once

#if defined(PLATFORM_ESP32)

#include "driver/rmt.h"
#include "common.h"
#include "rmtallocator.h"

class EncoderRMT {
public:
    virtual bool encode_bit(rmt_item32_t *rmtItem) = 0;
};

#define TRANSPONDER_RMT_SYMBOL_BUFFER_SIZE 64

enum eTransponderRMTState : uint8_t {
    TRMT_STATE_UNINIT,
    TRMT_STATE_RETRY_INIT,
    TRMT_STATE_INIT,
    TRMT_STATE_READY,
};

class TransponderRMT {
public:
    TransponderRMT() :
        haveChannel(false),
        state(TRMT_STATE_UNINIT)
        {};

    void configurePeripheral(gpio_num_t gpio);
    bool init(uint32_t desired_resolution_hz, uint32_t carrier_hz, uint8_t carrier_duty);
    bool isInitialised() { return state >= TRMT_STATE_INIT; };

    void encode(EncoderRMT *encoder);
    void start();
    void deinit();

private:
    uint32_t resolutionHz;

    bool haveChannel;
    rmt_channel_t channel;

    gpio_num_t gpio;
    eTransponderRMTState state;

    uint8_t rmtItemCount;
    rmt_item32_t rmtItems[TRANSPONDER_RMT_SYMBOL_BUFFER_SIZE];
};

#endif
