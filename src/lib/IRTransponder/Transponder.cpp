#if defined(TARGET_UNIFIED_RX) && defined(PLATFORM_ESP32)

#include "Transponder.h"

void Transponder::init(rmt_channel_t rmtChannel, gpio_num_t gpio, uint32_t id)
{
  this->rmtChannel = rmtChannel;
  this->gpio = gpio;
  this->id = id;

  initRMT();
  generateBitStream();
  encodeData();
}

void Transponder::startTransmission()
{
  if (rmt_wait_tx_done(rmtChannel, 0) == ESP_OK)
  {
    rmt_write_items(rmtChannel, rmtBits, NBITS, false);
  }
}

void Transponder::initRMT() {
  rmt_config_t config;

  config.channel = rmtChannel;
  config.rmt_mode = RMT_MODE_TX;
  config.gpio_num = gpio;
  config.mem_block_num = 1;
  config.clk_div = APB_CLK_FREQ/(BITRATE * 16);
  config.tx_config.loop_en = false;
  config.tx_config.idle_level = RMT_IDLE_LEVEL_LOW;
  config.tx_config.idle_output_en = true;
#if !defined(CARRIER)
  config.tx_config.carrier_en = false;
#else
  config.tx_config.carrier_en = true;
  config.tx_config.carrier_duty_percent = 50;
  config.tx_config.carrier_freq_hz = 5000000;
  config.tx_config.carrier_level = RMT_CARRIER_LEVEL_HIGH;
#endif

  rmt_config(&config);
  rmt_driver_install(rmtChannel, 0, 0);
}

void Transponder::encodeData()
{
  for (int i = 0; i < NBITS; i++)
  {
    if (bitStream & ((uint64_t)1 << NBITS))
    {
      // encode logic 0 as 3/16 of bit time
      rmtBits[i].duration0 = 3;
      rmtBits[i].level0 = 1;
      rmtBits[i].duration1 = 13;
      rmtBits[i].level1 = 0;
    }
    else
    {
      // encode logic 1 as off for bit time
      rmtBits[i].duration0 = 8;
      rmtBits[i].level0 = 0;
      rmtBits[i].duration1 = 8;
      rmtBits[i].level1 = 0;
    }

    bitStream <<= 1;
  }
}

//
// calculate CRC-8 with polynom 0x07 and init crc 0x00
//
uint8_t Transponder::crc8(uint8_t *data, uint8_t nBytes) {
  uint8_t crc = 0x00; 

  for (uint8_t i = 0 ; i < nBytes;) {
    crc ^= data[i++]; 

    for (uint8_t n = 0; n < 8; n++) {
      if (crc & 0x80)
        crc = (crc << 1) ^ 0x07;
      else
        crc <<= 1;
    }
  }

  return crc;
}

// 
// generate 44 bit sequence for given ID
//
void Transponder::generateBitStream() {
  uint8_t byte;
  uint64_t mask;

  // add crc
  id |= crc8((uint8_t *)&id, 3) << 24;

  bitStream = 0;

  for (uint8_t i = 0; i < 4; i++) {
    // start bit
    bitStream |= 1; bitStream <<= 1;

    byte = ((uint8_t *)&id)[i];
    mask = 0x80;

    // 8 data bits, reverse logic as per IRDA spec
    for (uint8_t n = 0; n < 8; n++) {
      if (!(byte & mask))
        bitStream |= 1;

      bitStream <<= 1;
      mask >>= 1;
    }

    // count number of bits in byte for parity bit
    if (__builtin_popcount(byte) % 2 == 0)
      bitStream |= 1;
    bitStream <<= 1;

    //stop bit
    bitStream <<= 1;
  }
  bitStream >>= 1;

  // bitstream bit set now represents logic 0
}

#endif
