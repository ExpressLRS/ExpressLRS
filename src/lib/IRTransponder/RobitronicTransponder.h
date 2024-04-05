#pragma once

#if defined(TARGET_UNIFIED_RX) && defined(PLATFORM_ESP32)

#include "driver/rmt.h"

#define NBITS   44
#define BITRATE 115200

#undef CARRIER

class RobitronicTransponder
{
public:
  RobitronicTransponder() {};
  ~RobitronicTransponder() {};

  void init(rmt_channel_t, gpio_num_t, uint32_t);
  void startTransmission();
  void deinit() { rmt_driver_uninstall(rmtChannel); };

protected:
  rmt_channel_t rmtChannel;
  gpio_num_t gpio;
  uint32_t id;
  uint64_t bitStream;

  rmt_item32_t rmtBits[NBITS];
  
  void initRMT();
  void encodeData();
  uint8_t crc8(uint8_t *, uint8_t);
  void generateBitStream();
};

#endif
