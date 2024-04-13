//
// Authors:
// * Mickey (mha1, initial RMT implementation)
// * Dominic Clifton (hydra, refactoring for multiple-transponder systems, iLap support)
//
#if defined(TARGET_UNIFIED_RX) && defined(PLATFORM_ESP32)

#include <Arduino.h>
#include "logging.h"
#include "RobitronicTransponder.h"

#define NBITS 44
#define BYTES 4
#define BITRATE 115200
#define BIT_PERIODS 16
#define CARRIER_HZ 0
#define CARRIER_DUTY 0

bool RobitronicTransponder::init()
{
    if (!transponderRMT->init(BITRATE * BIT_PERIODS, CARRIER_HZ, CARRIER_DUTY))
    {
        return false;
    }

    // generate unique transpoder ID (24Bit)
    uint32_t transponderID = ((uint32_t)firmwareOptions.uid[0] << 16) +
                             ((uint32_t)firmwareOptions.uid[1] << 8) +
                             ((uint32_t)firmwareOptions.uid[2]);

    DBGLN("RobitronicTransponder::init, id: 0x%x", transponderID);

    encoder->encode(transponderRMT, transponderID);

    return true;
}

void RobitronicTransponder::deinit()
{
    DBGLN("RobitronicTransponder::deinit");
    transponderRMT->deinit();
}

void RobitronicTransponder::startTransmission()
{
    transponderRMT->start();
}

void RobitronicEncoder::encode(TransponderRMT *transponderRMT, uint32_t id)
{
    generateBitStream(id);
    bits_encoded = 0;
    transponderRMT->encode(this);
}

bool RobitronicEncoder::encode_bit(rmt_item32_t *rmtItem)
{

    uint8_t bit = (bitStream & ((uint64_t)1 << NBITS)) > 0 ? 1 : 0;
    DBGVLN("RobitronicEncoder::encode_bit, index: %d, bit: %d", bits_encoded, bit);

    if (bit)
    {
        // encode logic 0 as 3/16 of bit time
        rmtItem->duration0 = 3;
        rmtItem->level0 = 1;
        rmtItem->duration1 = 13;
        rmtItem->level1 = 0;
    }
    else
    {
        // encode logic 1 as off for bit time
        rmtItem->duration0 = 8;
        rmtItem->level0 = 0;
        rmtItem->duration1 = 8;
        rmtItem->level1 = 0;
    }

    bitStream <<= 1;

    bits_encoded++;

    bool done = bits_encoded == NBITS;

    return done;
}

/**
 * @brief calculate CRC-8 with polynom 0x07 and init crc 0x00
 */
uint8_t RobitronicEncoder::crc8(uint8_t *data, uint8_t nBytes)
{
    uint8_t crc = 0x00;

    for (uint8_t i = 0; i < nBytes;)
    {
        crc ^= data[i++];

        for (uint8_t n = 0; n < 8; n++)
        {
            if (crc & 0x80)
            {
                crc = (crc << 1) ^ 0x07;
            }
            else
            {
                crc <<= 1;
            }
        }
    }

    DBGVLN("RobitronicEncoder::crc8, crc: %d", crc);

    return crc;
}

/**
 * @brief generate 44 bit sequence for given ID
 */
void RobitronicEncoder::generateBitStream(uint32_t id)
{
    uint8_t byte;
    uint64_t mask;

    // add crc
    id |= crc8((uint8_t *)&id, 3) << 24;

    bitStream = 0;

    for (uint8_t i = 0; i < BYTES; i++)
    {
        // start bit
        bitStream |= 1;
        bitStream <<= 1;

        byte = ((uint8_t *)&id)[i];
        mask = 0x80;

        // 8 data bits, reverse logic as per IRDA spec
        for (uint8_t n = 0; n < 8; n++)
        {
            if (!(byte & mask))
            {
                bitStream |= 1;
            }

            bitStream <<= 1;
            mask >>= 1;
        }

        // count number of bits in byte for parity bit
        if (__builtin_popcount(byte) % 2 == 0)
        {
            bitStream |= 1;
        }
        bitStream <<= 1;

        // stop bit
        bitStream <<= 1;
    }
    bitStream >>= 1;

    // bitstream bit set now represents logic 0
}

#endif
