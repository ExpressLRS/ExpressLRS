//
// Author: Dominic Clifton
//

#pragma once

#include "options.h"
#include "common.h"

#include "Transponder.h"
#include "TransponderRMT.h"

#if defined(TARGET_UNIFIED_RX) && defined(PLATFORM_ESP32)

class ILapEncoder : public EncoderRMT {
public:
    virtual bool encode_bit(rmt_item32_t *rmtItem);
    void encode(TransponderRMT *transponderRMT, uint64_t data);

private:
    uint8_t bits_encoded;
    uint64_t bitStream;

    void generateBitStream(uint64_t id);
};

class ILapTransponder : public TransponderSystem {
public:
    ILapTransponder(TransponderRMT *transponderRMT) {
        this->transponderRMT = transponderRMT;
        this->encoder = new ILapEncoder();
    };
    ~ILapTransponder() { deinit(); };

    virtual void init();
    virtual void startTransmission();

protected:
    virtual void deinit() { transponderRMT->deinit(); };
    TransponderRMT *transponderRMT;
    ILapEncoder *encoder;
};

#endif
