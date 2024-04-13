//
// Author: Dominic Clifton
//

#pragma once

#include "options.h"
#include "common.h"

#include "Transponder.h"
#include "TransponderRMT.h"

#if defined(TARGET_UNIFIED_RX) && defined(PLATFORM_ESP32)

/**
 * @brief Encoder for an ILap RMT transponder
 */
class ILapEncoder : public EncoderRMT {
public:
    virtual bool encode_bit(rmt_item32_t *rmtItem);
    void encode(TransponderRMT *transponderRMT, uint64_t data);

private:
    uint8_t bits_encoded;
    uint64_t bitStream;

    void generateBitStream(uint64_t id);
};

/**
 * @brief ILap transponder system.
 */
class ILapTransponder : public TransponderSystem {
public:
    /**
     * @brief Create an instance
     * @param transponderRMT the RMT transponder implementation to use.
     */
    ILapTransponder(TransponderRMT *transponderRMT) {
        this->transponderRMT = transponderRMT;
        this->encoder = new ILapEncoder();
    };
    ~ILapTransponder() { deinit(); };

    virtual bool init();
    virtual bool isInitialised() { return transponderRMT->isInitialised(); };
    virtual void startTransmission(uint32_t &intervalMs);

protected:
    virtual void deinit();
    TransponderRMT *transponderRMT;
    ILapEncoder *encoder;
};

#endif
