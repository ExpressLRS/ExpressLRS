//
// Authors: 
// * Mickey (mha1, initial RMT implementation)
// * Dominic Clifton (hydra, refactoring for multiple-transponder systems, iLap support)
//
#pragma once

#include "options.h"
#include "common.h"

#include "Transponder.h"
#include "TransponderRMT.h"

#if defined(TARGET_UNIFIED_RX) && defined(PLATFORM_ESP32)

/**
 * @brief Encoder for a Robitronic RMT transponder
 */
class RobitronicEncoder : public EncoderRMT {
public:
    virtual bool encode_bit(rmt_item32_t *rmtItem);
    void encode(TransponderRMT *transponderRMT, uint32_t id);

private:
    uint64_t bitStream;
    uint8_t bits_encoded;

    uint8_t crc8(uint8_t *, uint8_t);
    void generateBitStream(uint32_t id);
};

/**
 * @brief Robitronic transponder system.
 */
class RobitronicTransponder : public TransponderSystem {
public:
    /**
     * @brief Create an instance
     * @param transponderRMT the RMT transponder implementation to use.
     */
    RobitronicTransponder(TransponderRMT *transponderRMT) {
        this->transponderRMT = transponderRMT;
        this->encoder = new RobitronicEncoder();
    };
    ~RobitronicTransponder() { deinit(); };

    virtual bool init();
    virtual bool isInitialised() { return transponderRMT->isInitialised(); };
    virtual void startTransmission(uint32_t &intervalMs);

protected:
    virtual void deinit();
    TransponderRMT *transponderRMT;
    RobitronicEncoder *encoder;
};

#endif
