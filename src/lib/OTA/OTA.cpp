/**
 * This file is part of ExpressLRS
 * See https://github.com/AlessandroAU/ExpressLRS
 *
 * This file provides utilities for packing and unpacking the data to
 * be sent over the radio link.
 */

#include "OTA.h"
#include <assert.h>


static_assert(sizeof(OTA_Packet_s) == OTA_PACKET_SIZE, "OTA packet stuct is invalid!");


static inline uint8_t ICACHE_RAM_ATTR HybridWideNonceToSwitchIndex(uint8_t const nonce)
{
    // Returns the sequence (0 to 7, then 0 to 7 rotated left by 1):
    // 0, 1, 2, 3, 4, 5, 6, 7,
    // 1, 2, 3, 4, 5, 6, 7, 0
    // Because telemetry can occur on every 2, 4, 8, 16, 32, 64, 128th packet
    // this makes sure each of the 8 values is sent at least once every 16 packets
    // regardless of the TLM ratio
    // Index 7 also can never fall on a telemetry slot
    return ((nonce & 0b111) + ((nonce >> 3) & 0b1)) % 8;
}

#if TARGET_TX || defined(UNIT_TEST)

// Current ChannelData generator function being used by TX
PackChannelData_t PackChannelData;

static void ICACHE_RAM_ATTR PackChannelDataHybridCommon(OTA_Packet_s * const otaPktPtr, CRSF const * const crsf)
{
    otaPktPtr->type = RC_DATA_PACKET;
#if defined(DEBUG_RCVR_LINKSTATS)
    // Incremental packet counter for verification on the RX side, 32 bits shoved into CH1-CH4
    static uint32_t packetCnt;
    otaPktPtr->dbg_linkstats.packetNum = htobe32(packetCnt);
    otaPktPtr->dbg_linkstats.free1 = 0;
    otaPktPtr->dbg_linkstats.free2 = 0;
    ++packetCnt;
#else
    // CRSF input is 11bit and OTA will carry only 10b => LSB is dropped
    otaPktPtr->rc.ch0High = ((crsf->ChannelDataIn[0]) >> 3);
    otaPktPtr->rc.ch0Low  = crsf->ChannelDataIn[0] >> 1;
    otaPktPtr->rc.ch1High = ((crsf->ChannelDataIn[1]) >> 3);
    otaPktPtr->rc.ch1Low  = crsf->ChannelDataIn[1] >> 1;
    otaPktPtr->rc.ch2High = ((crsf->ChannelDataIn[2]) >> 3);
    otaPktPtr->rc.ch2Low  = crsf->ChannelDataIn[2] >> 1;
    otaPktPtr->rc.ch3High = ((crsf->ChannelDataIn[3]) >> 3);
    otaPktPtr->rc.ch3Low  = crsf->ChannelDataIn[3] >> 1;
#endif /* !DEBUG_RCVR_LINKSTATS */
}

/**
 * Hybrid switches packet encoding for sending over the air
 *
 * Analog channels are reduced to 10 bits to allow for switch encoding
 * Switch[0] is sent on every packet.
 * A 3 bit switch index and 3-4 bit value is used to send the remaining switches
 * in a round-robin fashion.
 *
 * Inputs: crsf.ChannelDataIn, crsf.currentSwitches
 * Outputs: OTA_Packet_s, side-effects the sentSwitch value
 */
// The next switch index to send, where 0=AUX2 and 6=AUX8
static uint8_t Hybrid8NextSwitchIndex;
#if defined(UNIT_TEST)
void OtaSetHybrid8NextSwitchIndex(uint8_t idx) { Hybrid8NextSwitchIndex = idx; }
#endif
void ICACHE_RAM_ATTR GenerateChannelDataHybrid8(OTA_Packet_s * const otaPktPtr, CRSF const * const crsf,
                                                bool const TelemetryStatus, uint8_t const nonce,
                                                uint8_t const tlmDenom)
{
    (void)nonce;
    (void)tlmDenom;

    PackChannelDataHybridCommon(otaPktPtr, crsf);

    // Actually send switchIndex - 1 in the packet, to shift down 1-7 (0b111) to 0-6 (0b110)
    // If the two high bits are 0b11, the receiver knows it is the last switch and can use
    // that bit to store data
    uint8_t bitclearedSwitchIndex = Hybrid8NextSwitchIndex;
    uint8_t value;
    // AUX8 is High Resolution 16-pos (4-bit)
    if (bitclearedSwitchIndex == 6)
        value = CRSF_to_N(crsf->ChannelDataIn[6 + 1 + 4], 16);
    else
    {
        // AUX2-7 are Low Resolution, "7pos" 6+center (3-bit)
        // The output is mapped evenly across 6 output values (0-5)
        // with a special value 7 indicating the middle so it works
        // with switches with a middle position as well as 6-position
        const uint16_t CHANNEL_BIN_COUNT = 6;
        const uint16_t CHANNEL_BIN_SIZE = (CRSF_CHANNEL_VALUE_MAX - CRSF_CHANNEL_VALUE_MIN) / CHANNEL_BIN_COUNT;
        uint16_t ch = crsf->ChannelDataIn[bitclearedSwitchIndex + 1 + 4];
        // If channel is within 1/4 a BIN of being in the middle use special value 7
        if (ch < (CRSF_CHANNEL_VALUE_MID-CHANNEL_BIN_SIZE/4)
            || ch > (CRSF_CHANNEL_VALUE_MID+CHANNEL_BIN_SIZE/4))
            value = CRSF_to_N(ch, CHANNEL_BIN_COUNT);
        else
            value = 7;
    } // If not 16-pos

    otaPktPtr->rc.switches =
        TelemetryStatus << 7 |
        // switch 0 is one bit sent on every packet - intended for low latency arm/disarm
        CRSF_to_BIT(crsf->ChannelDataIn[4]) << 6 |
        // tell the receiver which switch index this is
        bitclearedSwitchIndex << 3 |
        // include the switch value
        value;

    // update the sent value
    Hybrid8NextSwitchIndex = (bitclearedSwitchIndex + 1) % 7;
}

/**
 * Return the OTA value respresentation of the switch contained in ChannelDataIn
 * Switches 1-6 (AUX2-AUX7) are 6 or 7 bit depending on the lowRes parameter
 */
static uint8_t ICACHE_RAM_ATTR HybridWideSwitchToOta(CRSF const * const crsf, uint8_t const switchIdx, bool const lowRes)
{
    uint16_t ch = crsf->ChannelDataIn[switchIdx + 4];
    uint8_t binCount = (lowRes) ? 64 : 128;
    ch = CRSF_to_N(ch, binCount);
    if (lowRes)
        return ch & 0b111111; // 6-bit
    else
        return ch & 0b1111111; // 7-bit
}

/**
 * HybridWide switches packet encoding for sending over the air
 *
 * Analog channels are reduced to 10 bits to allow for switch encoding
 * Switch[0] is sent on every packet.
 * A 6 or 7 bit switch value is used to send the remaining switches
 * in a round-robin fashion.
 *
 * Inputs: crsf.ChannelDataIn, crsf.LinkStatistics.uplink_TX_Power
 * Outputs: OTA_Packet_s
 **/
void ICACHE_RAM_ATTR GenerateChannelDataHybridWide(OTA_Packet_s * const otaPktPtr, CRSF const * const crsf,
                                                   bool const TelemetryStatus, uint8_t const nonce,
                                                   uint8_t const tlmDenom)
{
    PackChannelDataHybridCommon(otaPktPtr, crsf);

    uint8_t telemBit = TelemetryStatus << 6;
    uint8_t nextSwitchIndex = HybridWideNonceToSwitchIndex(nonce);
    uint8_t value;
    // Using index 7 means the telemetry bit will always be sent in the packet
    // preceding the RX's telemetry slot for all tlmDenom >= 8
    // For more frequent telemetry rates, include the bit in every
    // packet and degrade the value to 6-bit
    // (technically we could squeeze 7-bits in for 2 channels with tlmDenom=4)
    if (nextSwitchIndex == 7)
    {
        value = telemBit | crsf->LinkStatistics.uplink_TX_Power;
    }
    else
    {
        bool telemInEveryPacket = (tlmDenom < 8);
        value = HybridWideSwitchToOta(crsf, nextSwitchIndex + 1, telemInEveryPacket);
        if (telemInEveryPacket)
            value |= telemBit;
    }

    otaPktPtr->rc.switches =
        // switch 0 is one bit sent on every packet - intended for low latency arm/disarm
        CRSF_to_BIT(crsf->ChannelDataIn[4]) << 7 |
        // include the switch value
        value;
}
#endif


#if TARGET_RX || defined(UNIT_TEST)

// Current ChannelData unpacker function being used by RX
UnpackChannelData_t UnpackChannelData;

#if defined(DEBUG_RCVR_LINKSTATS)
// Sequential PacketID from the TX
uint32_t debugRcvrLinkstatsPacketId;
#endif

static void ICACHE_RAM_ATTR UnpackChannelDataHybridCommon(OTA_Packet_s const * const otaPktPtr, CRSF * const crsf)
{
    // The analog channels
#if defined(DEBUG_RCVR_LINKSTATS)
    debugRcvrLinkstatsPacketId = be32toh(otaPktPtr->dbg_linkstats.packetNum);
#else
    crsf->PackedRCdataOut.ch0 = ((uint16_t)otaPktPtr->rc.ch0High << 3) + ((uint16_t)otaPktPtr->rc.ch0Low << 1);
    crsf->PackedRCdataOut.ch1 = ((uint16_t)otaPktPtr->rc.ch1High << 3) + ((uint16_t)otaPktPtr->rc.ch1Low << 1);
    crsf->PackedRCdataOut.ch2 = ((uint16_t)otaPktPtr->rc.ch2High << 3) + ((uint16_t)otaPktPtr->rc.ch2Low << 1);
    crsf->PackedRCdataOut.ch3 = ((uint16_t)otaPktPtr->rc.ch3High << 3) + ((uint16_t)otaPktPtr->rc.ch3Low << 1);
#endif
}

/**
 * Hybrid switches decoding of over the air data
 *
 * Hybrid switches uses 10 bits for each analog channel,
 * 2 bits for the low latency switch[0]
 * 3 bits for the round-robin switch index and 2 bits for the value
 *
 * Input: Buffer
 * Output: crsf->PackedRCdataOut
 * Returns: TelemetryStatus bit
 */
bool ICACHE_RAM_ATTR UnpackChannelDataHybridSwitch8(OTA_Packet_s const * const otaPktPtr, CRSF * const crsf,
                                                    uint8_t const nonce, uint8_t const tlmDenom)
{
    (void)nonce;
    (void)tlmDenom;

    const uint8_t switchByte = otaPktPtr->rc.switches;
    UnpackChannelDataHybridCommon(otaPktPtr, crsf);

    // The low latency switch
    crsf->PackedRCdataOut.ch4 = BIT_to_CRSF((switchByte & 0b01000000) >> 6);

    // The round-robin switch, switchIndex is actually index-1
    // to leave the low bit open for switch 7 (sent as 0b11x)
    // where x is the high bit of switch 7
    uint8_t switchIndex = (switchByte & 0b111000) >> 3;
    uint16_t switchValue = SWITCH3b_to_CRSF(switchByte & 0b111);

    switch (switchIndex) {
    case 0:
        crsf->PackedRCdataOut.ch5 = switchValue;
        break;
    case 1:
        crsf->PackedRCdataOut.ch6 = switchValue;
        break;
    case 2:
        crsf->PackedRCdataOut.ch7 = switchValue;
        break;
    case 3:
        crsf->PackedRCdataOut.ch8 = switchValue;
        break;
    case 4:
        crsf->PackedRCdataOut.ch9 = switchValue;
        break;
    case 5:
        crsf->PackedRCdataOut.ch10 = switchValue;
        break;
    case 6:   // Because AUX1 (index 0) is the low latency switch, the low bit
    case 7:   // of the switchIndex can be used as data, and arrives as index "6"
        crsf->PackedRCdataOut.ch11 = N_to_CRSF(switchByte & 0b1111, 15);
        break;
    }

    // TelemetryStatus bit
    return switchByte & (1 << 7);
}

/**
 * HybridWide switches decoding of over the air data
 *
 * Hybrid switches uses 10 bits for each analog channel,
 * 1 bits for the low latency switch[0]
 * 6 or 7 bits for the round-robin switch
 * 1 bit for the TelemetryStatus, which may be in every packet or just idx 7
 * depending on TelemetryRatio
 *
 * Output: crsf.PackedRCdataOut, crsf.LinkStatistics.uplink_TX_Power
 * Returns: TelemetryStatus bit
 */
bool ICACHE_RAM_ATTR UnpackChannelDataHybridWide(OTA_Packet_s const * const otaPktPtr, CRSF * const crsf,
                                                 uint8_t const nonce, uint8_t const tlmDenom)
{
    static bool TelemetryStatus = false;
    const uint8_t switchByte = otaPktPtr->rc.switches;
    UnpackChannelDataHybridCommon(otaPktPtr, crsf);

    // The low latency switch (AUX1)
    crsf->PackedRCdataOut.ch4 = BIT_to_CRSF((switchByte & 0b10000000) >> 7);

    // The round-robin switch, 6-7 bits with the switch index implied by the nonce
    uint8_t switchIndex = HybridWideNonceToSwitchIndex(nonce);
    bool telemInEveryPacket = (tlmDenom < 8);
    if (telemInEveryPacket || switchIndex == 7)
          TelemetryStatus = (switchByte & 0b01000000) >> 6;
    if (switchIndex == 7)
    {
        crsf->LinkStatistics.uplink_TX_Power = switchByte & 0b111111;
    }
    else
    {
        uint8_t bins;
        uint16_t switchValue;
        if (telemInEveryPacket)
        {
            bins = 63;
            switchValue = switchByte & 0b111111; // 6-bit
        }
        else
        {
            bins = 127;
            switchValue = switchByte & 0b1111111; // 7-bit
        }

        switchValue = N_to_CRSF(switchValue, bins);
        switch (switchIndex) {
            case 0:
                crsf->PackedRCdataOut.ch5 = switchValue;
                break;
            case 1:
                crsf->PackedRCdataOut.ch6 = switchValue;
                break;
            case 2:
                crsf->PackedRCdataOut.ch7 = switchValue;
                break;
            case 3:
                crsf->PackedRCdataOut.ch8 = switchValue;
                break;
            case 4:
                crsf->PackedRCdataOut.ch9 = switchValue;
                break;
            case 5:
                crsf->PackedRCdataOut.ch10 = switchValue;
                break;
            case 6:
                crsf->PackedRCdataOut.ch11 = switchValue;
                break;
        }
    }

    return TelemetryStatus;
}
#endif

OtaSwitchMode_e OtaSwitchModeCurrent;
void OtaSetSwitchMode(OtaSwitchMode_e const switchMode)
{
    switch(switchMode)
    {
    case sm1Bit:
    case smHybrid:
    default:
        #if defined(TARGET_TX) || defined(UNIT_TEST)
        PackChannelData = &GenerateChannelDataHybrid8;
        #endif
        #if defined(TARGET_RX) || defined(UNIT_TEST)
        UnpackChannelData = &UnpackChannelDataHybridSwitch8;
        #endif
        break;
    case smHybridWide:
        #if defined(TARGET_TX) || defined(UNIT_TEST)
        PackChannelData = &GenerateChannelDataHybridWide;
        #endif
        #if defined(TARGET_RX) || defined(UNIT_TEST)
        UnpackChannelData = &UnpackChannelDataHybridWide;
        #endif
        break;
    }

    OtaSwitchModeCurrent = switchMode;
}


