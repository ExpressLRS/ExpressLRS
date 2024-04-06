//
// Authors: 
// * Mickey (mha1, initial RMT implementation)
// * Dominic Clifton (hydra, refactoring for multiple-transponder systems, iLap support)
//

#pragma once

#if defined(PLATFORM_ESP32)

#include "driver/rmt.h"
#include "common.h"

class EncoderRMT {
public:
    virtual bool encode_bit(rmt_item32_t *rmtItem) = 0;
};

#define TRANSPONDER_RMT_SYMBOL_BUFFER_SIZE 64

class TransponderRMT {
public:
    void configurePeripheral(rmt_channel_t channel, gpio_num_t gpio);
    void init(uint32_t desired_resolution_hz, uint32_t carrier_hz, uint8_t carrier_duty);

    void encode(EncoderRMT *encoder);
    void start();
    void deinit();

private:
    uint32_t resolutionHz;
    rmt_channel_t channel;
    gpio_num_t gpio;

    uint8_t rmtItemCount;
    rmt_item32_t rmtItems[TRANSPONDER_RMT_SYMBOL_BUFFER_SIZE];
};

#endif
