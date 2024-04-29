//
// Authors:
// * Mickey (mha1, initial RMT implementation)
// * Dominic Clifton (hydra, refactoring for multiple-transponder systems, iLap support)
//
#if defined(TARGET_UNIFIED_RX) && defined(PLATFORM_ESP32)

#include <Arduino.h>
#include "logging.h"
#include "random.h"
#include "RobitronicTransponder.h"

#define NBITS 44
#define BYTES 4
#define BITRATE 100000
#define BIT_PERIODS 10
#define CARRIER_HZ 0
#define CARRIER_DUTY 0
#define INTERVAL_JITTER_MS 4
#define MINIMUM_INTERVAL_MS 1

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

void RobitronicTransponder::startTransmission(uint32_t &intervalMs)
{
    transponderRMT->start();

    // Original robitronic transponders have an random interval between the END of one transmission and the START of the next of between 0.5ms and 4.5ms
    // which gives a 4ms jitter difference, however the resolution here is one millisecond, and we need to account for the time it takes to do a transmission (~400uS)
    // so the best we can do is from 1ms to 5ms
    intervalMs = MINIMUM_INTERVAL_MS + rngN(INTERVAL_JITTER_MS + 1);
}

void RobitronicEncoder::encode(TransponderRMT *transponderRMT, uint32_t id)
{
    generateBitStream(id);
    bits_encoded = 0;
    transponderRMT->encode(this);
}

bool RobitronicEncoder::encode_bit(rmt_item32_t *rmtItem)
{
    uint8_t bit = (bitStream & ((uint64_t)1 << (NBITS-1))) > 0 ? 1 : 0;
    DBGVLN("RobitronicEncoder::encode_bit, index: %d, bit: %d", bits_encoded, bit);

    if((bits_encoded + 1) % 11 == 0) {
        // encode stop bit as IR off for 1/2 bit time
        rmtItem->duration0 = 3;
        rmtItem->level0 = 0;
        rmtItem->duration1 = 2;
        rmtItem->level1 = 0;
    } 
    else 
    {
        if (bit)
        {
            // encode logic 1 as off for bit time
            rmtItem->duration0 = 5;
            rmtItem->level0 = 0;
            rmtItem->duration1 = 5;
            rmtItem->level1 = 0;
        }
        else
        {
            // encode logic 0 as 2/10 of bit time
            rmtItem->duration0 = 2;
            rmtItem->level0 = 1;
            rmtItem->duration1 = 8;
            rmtItem->level1 = 0;
        }
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
 * @brief generate 44 bit sequence for given ID. The bit sequence will be encoded in encode_bit()
 * for IR output (reverse logic)
 * logic 0 -> 2us IR pulse followed by 8us silence
 * logic 1 -> 10us silence
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
        // start bit (logic 0, IR pulse)
        bitStream <<= 1;

        byte = ((uint8_t *)&id)[i];
        mask = 0x80;

        // 8 data bits (logic 0, IR pulse - logic 1, no IR pulse)
        for (uint8_t n = 0; n < 8; n++)
        {
            if (byte & mask)
            {
                bitStream |= 1;
            }

            bitStream <<= 1;
            mask >>= 1;
        }

        // count number of bits in byte for parity bit, odd = logic 1 (no IR  pulse)
        if (__builtin_popcount(byte) % 2 != 0)
        {
            bitStream |= 1;
        }
        bitStream <<= 1;

        // stop bit logic 1 (no IR pulse)
        bitStream |= 1;
        bitStream <<= 1;
    }
    bitStream >>= 1;
}

#endif
