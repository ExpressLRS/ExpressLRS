#pragma once

#include "options.h"
#include "common.h"

#include "TransponderRMT.h"

#if defined(TARGET_UNIFIED_RX) && defined(PLATFORM_ESP32)

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

class RobitronicTransponder {
public:
  RobitronicTransponder(TransponderRMT *transponderRMT) {
      this->transponderRMT = transponderRMT;
      this->encoder = new RobitronicEncoder();
  };
  ~RobitronicTransponder() {};

  void init();
  void startTransmission();
  void deinit() { transponderRMT->deinit(); };

protected:
    TransponderRMT *transponderRMT;
    RobitronicEncoder *encoder;
};

#endif
