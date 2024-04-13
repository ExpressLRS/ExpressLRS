//
// Authors: 
// * Dominic Clifton (hydra, initial support)
//
// Based on the RobitronicTransponder implementation
//

#if defined(TARGET_UNIFIED_RX) && defined(PLATFORM_ESP32)

#include <Arduino.h>
#include "config.h"
#include "logging.h"
#include "ILapTransponder.h"

#define NBITS ((1 + 8 + 1) * 6) // 60
#define BYTES 6
#define BITRATE 38400
#define BIT_PERIODS 12
#define CARRIER_HZ 460800
#define CARRIER_DUTY 50

// iLap scope measurements
// 1 pulse = 2.175us (@500ns/div)
// 3 pulses = 6.51us (@1us/div)
// 8 pulses = 21.75us
// 12 pulses = 26.1us (extrapolated)
//
// ilap signal has 11 pulses of high+low, then low+low gap pulses for a '1', i.e. 12 periods at 50% carrier duty
// total signal length is 1.544ms (@200us/div)
// signal has 60 groups of 11 pulses + 1 gap period
// 1544ms / 60 = 25ms/pulse
// 1000000 / 1544 = 647, 647 * 60 = 38820 which is pretty close to 38400
// or using extrapolated values:
// 26.1 * 60 = 1566
// 1000000 / 1566 = 638.5696040868455 = 38314.176
// assuming measurement error, clock drift or signal error caused by rounding on iLap transponder (which has a 7.372Mhz crystal)
// 38400 * 12 = 460800 (carrier)
//
// Note: an ESP32 with an 80Mhz APB driving the RMT and a carrier frequency of 460.800kHz requires a clock divider of 173.11 (not a round number)
// so 80Mhz / 173 = of 462.427kHz, not 460.800kHz, but an actual iLap receiver is able to receiver the signal.
// and 462427 / 12 = 38535.583bps, not 38400bps.
// thus, the code could be improved to calculate the bitrate based on the ACTUAL resolution of the RMT.
// actual RMT resolution should be used to set the carrier_hz and the bitrate

bool ILapTransponder::init()
{
    if (!transponderRMT->init(BITRATE * BIT_PERIODS, CARRIER_HZ, CARRIER_DUTY))
    {
        return false;
    }

    // uint64_t data = 0xFF0FEDCBA987; // NOT A VALID TRANSPONDER CODE
    uint64_t data = config.GetIRiLapCode();

    DBGLN("ILapTransponder::init, data: 0x%x", data);

    encoder->encode(transponderRMT, data);

    return true;
}

void ILapTransponder::deinit()
{
    DBGLN("ILapTransponder::deinit");
    transponderRMT->deinit();
}

void ILapTransponder::startTransmission()
{
    transponderRMT->start();
}

void ILapEncoder::encode(TransponderRMT *transponderRMT, uint64_t data)
{
    generateBitStream(data);
    bits_encoded = 0;
    transponderRMT->encode(this);
}

bool ILapEncoder::encode_bit(rmt_item32_t *rmtItem)
{

    uint8_t bit = (bitStream & ((uint64_t)1 << NBITS)) > 0 ? 1 : 0;
    DBGVLN("ILapEncoder::encode_bit, index: %d, bit: %d", bits_encoded, bit);

    if (bit)
    {
        rmtItem->duration0 = 11;
        rmtItem->level0 = 1;
        rmtItem->duration1 = 1;
        rmtItem->level1 = 0;
    }
    else
    {
        rmtItem->duration0 = 6;
        rmtItem->level0 = 0;
        rmtItem->duration1 = 6;
        rmtItem->level1 = 0;
    }

    bitStream <<= 1;

    bits_encoded++;

    bool done = bits_encoded == NBITS;

    return done;
}

void ILapEncoder::generateBitStream(uint64_t data)
{
    uint8_t byte;
    uint64_t mask;

    bitStream = 0;

    for (uint8_t i = 0; i < BYTES; i++)
    {
        // start bit
        bitStream |= 1;
        bitStream <<= 1;

        byte = ((uint8_t *)&data)[BYTES - i - 1];
        DBGLN("ILapEncoder::generateBitStream, byte: 0x%x", byte);

        mask = 0x01; // start with LSB

        // 8 data bits, LSB first
        for (uint8_t n = 0; n < 8; n++)
        {
            uint8_t bit = 0;
            if (!(byte & mask))
            {
                bit |= 1;
            }

            DBGVLN("ILapEncoder::generateBitStream, byte_index: %d, bit_of_byte: %d, byte: %x, bit: %d", i, n, byte, bit);

            bitStream |= bit;
            bitStream <<= 1;
            mask <<= 1;
        }

        // no parity bit

        // stop bit
        bitStream <<= 1;
    }
    bitStream >>= 1;
}

#endif
