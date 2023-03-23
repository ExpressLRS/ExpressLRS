/**
 * This file is part of ExpressLRS
 * See https://github.com/AlessandroAU/ExpressLRS
 *
 * This file provides utilities for packing and unpacking the data to
 * be sent over the radio link.
 */

#include "OTA.h"
#include "common.h"
#include <assert.h>

static_assert(sizeof(OTA_Packet4_s) == OTA4_PACKET_SIZE, "OTA4 packet stuct is invalid!");
static_assert(sizeof(OTA_Packet8_s) == OTA8_PACKET_SIZE, "OTA8 packet stuct is invalid!");

bool OtaIsFullRes;
volatile uint8_t OtaNonce;
uint16_t OtaCrcInitializer;
OtaSwitchMode_e OtaSwitchModeCurrent;

// CRC
static Crc2Byte ota_crc;
ValidatePacketCrc_t OtaValidatePacketCrc;
GeneratePacketCrc_t OtaGeneratePacketCrc;

void OtaUpdateCrcInitFromUid()
{
    OtaCrcInitializer = (UID[4] << 8) | UID[5];
    OtaCrcInitializer ^= OTA_VERSION_ID;
}

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
PackChannelData_t OtaPackChannelData;
#if defined(DEBUG_RCVR_LINKSTATS)
static uint32_t packetCnt;
#endif

/******** Decimate 11bit to 10bit functions ********/
typedef uint32_t (*Decimate11to10_fn)(uint32_t ch11bit);

static uint32_t ICACHE_RAM_ATTR Decimate11to10_Limit(uint32_t ch11bit)
{
    // Limit 10-bit result to the range CRSF_CHANNEL_VALUE_MIN/MAX
    return CRSF_to_UINT10(constrain(ch11bit, CRSF_CHANNEL_VALUE_MIN, CRSF_CHANNEL_VALUE_MAX));
}

static uint32_t ICACHE_RAM_ATTR Decimate11to10_Div2(uint32_t ch11bit)
{
    // Simple divide-by-2 to discard the bit
    return ch11bit >> 1;
}

/***
 * @brief: Pack 4x 11-bit channel array into 4x 10 bit channel struct
 * @desc: Values are packed little-endianish such that bits A987654321 -> 87654321, 000000A9
 *        which is compatible with the 10-bit CRSF subset RC frame structure (0x17) in
 *        Betaflight, but depends on which decimate function is used if it is legacy or CRSFv3 10-bit
 *        destChannels4x10 must be zeroed before this call, the channels are ORed into it
 ***/
static void ICACHE_RAM_ATTR PackUInt11ToChannels4x10(uint32_t const * const src, OTA_Channels_4x10 * const destChannels4x10, Decimate11to10_fn decimate)
{
    const unsigned DEST_PRECISION = 10; // number of bits for each dest, must be <SRC
    uint8_t *dest = (uint8_t *)destChannels4x10;
    *dest = 0;
    unsigned destShift = 0;
    for (unsigned ch=0; ch<4; ++ch)
    {
        // Convert to DEST_PRECISION value
        unsigned chVal = decimate(src[ch]);

        // Put the low bits in any remaining dest capacity
        *dest++ |= chVal << destShift;

        // Shift the high bits down and place them into the next dest byte
        unsigned srcBitsLeft = DEST_PRECISION - 8 + destShift;
        *dest = chVal >> (DEST_PRECISION - srcBitsLeft);
        // Next dest should be shifted up by the bits consumed
        // if destShift == 8 then everything should reset for another set
        // but this code only expects to do the 4 channels -> 5 bytes evenly
        destShift = srcBitsLeft;
    }
}

static void ICACHE_RAM_ATTR PackChannelDataHybridCommon(OTA_Packet4_s * const ota4, const uint32_t *channelData)
{
    ota4->type = PACKET_TYPE_RCDATA;
#if defined(DEBUG_RCVR_LINKSTATS)
    // Incremental packet counter for verification on the RX side, 32 bits shoved into CH1-CH4
    ota4->dbg_linkstats.packetNum = packetCnt++;
#else
    // CRSF input is 11bit and OTA will carry only 10bit. Discard the Extended Limits (E.Limits)
    // range and use the full 10bits to carry only 998us - 2012us
    PackUInt11ToChannels4x10(&channelData[0], &ota4->rc.ch, &Decimate11to10_Limit);
    ota4->rc.ch4 = CRSF_to_BIT(channelData[4]);
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
 * Inputs: channelData, TelemetryStatus
 * Outputs: OTA_Packet4_s, side-effects the sentSwitch value
 */
// The next switch index to send, where 0=AUX2 and 6=AUX8
static uint8_t Hybrid8NextSwitchIndex;
#if defined(UNIT_TEST)
void OtaSetHybrid8NextSwitchIndex(uint8_t idx) { Hybrid8NextSwitchIndex = idx; }
#endif
void ICACHE_RAM_ATTR GenerateChannelDataHybrid8(OTA_Packet_s * const otaPktPtr, const uint32_t *channelData,
                                                bool const TelemetryStatus, uint8_t const tlmDenom)
{
    (void)tlmDenom;

    OTA_Packet4_s * const ota4 = &otaPktPtr->std;
    PackChannelDataHybridCommon(ota4, channelData);

    // Actually send switchIndex - 1 in the packet, to shift down 1-7 (0b111) to 0-6 (0b110)
    // If the two high bits are 0b11, the receiver knows it is the last switch and can use
    // that bit to store data
    uint8_t bitclearedSwitchIndex = Hybrid8NextSwitchIndex;
    uint8_t value;
    // AUX8 is High Resolution 16-pos (4-bit)
    if (bitclearedSwitchIndex == 6)
        value = CRSF_to_N(channelData[6 + 1 + 4], 16);
    else
    {
        // AUX2-7 are Low Resolution, "7pos" 6+center (3-bit)
        // The output is mapped evenly across 6 output values (0-5)
        // with a special value 7 indicating the middle so it works
        // with switches with a middle position as well as 6-position
        const uint16_t CHANNEL_BIN_COUNT = 6;
        const uint16_t CHANNEL_BIN_SIZE = (CRSF_CHANNEL_VALUE_MAX - CRSF_CHANNEL_VALUE_MIN) / CHANNEL_BIN_COUNT;
        uint16_t ch = channelData[bitclearedSwitchIndex + 1 + 4];
        // If channel is within 1/4 a BIN of being in the middle use special value 7
        if (ch < (CRSF_CHANNEL_VALUE_MID-CHANNEL_BIN_SIZE/4)
            || ch > (CRSF_CHANNEL_VALUE_MID+CHANNEL_BIN_SIZE/4))
            value = CRSF_to_N(ch, CHANNEL_BIN_COUNT);
        else
            value = 7;
    } // If not 16-pos

    ota4->rc.switches =
        TelemetryStatus << 6 |
        // tell the receiver which switch index this is
        bitclearedSwitchIndex << 3 |
        // include the switch value
        value;

    // update the sent value
    Hybrid8NextSwitchIndex = (bitclearedSwitchIndex + 1) % 7;
}

/**
 * Return the OTA value respresentation of the switch contained in ChannelData
 * Switches 1-6 (AUX2-AUX7) are 6 or 7 bit depending on the lowRes parameter
 */
static uint8_t ICACHE_RAM_ATTR HybridWideSwitchToOta(const uint32_t *channelData, uint8_t const switchIdx, bool const lowRes)
{
    uint16_t ch = channelData[switchIdx + 4];
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
 * Inputs: cchannelData, TelemetryStatus
 * Outputs: OTA_Packet4_s
 **/
void ICACHE_RAM_ATTR GenerateChannelDataHybridWide(OTA_Packet_s * const otaPktPtr, const uint32_t *channelData,
                                                   bool const TelemetryStatus, uint8_t const tlmDenom)
{
    OTA_Packet4_s * const ota4 = &otaPktPtr->std;
    PackChannelDataHybridCommon(ota4, channelData);

    uint8_t telemBit = TelemetryStatus << 6;
    uint8_t nextSwitchIndex = HybridWideNonceToSwitchIndex(OtaNonce);
    uint8_t value;
    // Using index 7 means the telemetry bit will always be sent in the packet
    // preceding the RX's telemetry slot for all tlmDenom >= 8
    // For more frequent telemetry rates, include the bit in every
    // packet and degrade the value to 6-bit
    // (technically we could squeeze 7-bits in for 2 channels with tlmDenom=4)
    if (nextSwitchIndex == 7)
    {
        value = telemBit | CRSF::LinkStatistics.uplink_TX_Power;
    }
    else
    {
        bool telemInEveryPacket = (tlmDenom < 8);
        value = HybridWideSwitchToOta(channelData, nextSwitchIndex + 1, telemInEveryPacket);
        if (telemInEveryPacket)
            value |= telemBit;
    }

    ota4->rc.switches = value;
}

static void ICACHE_RAM_ATTR GenerateChannelData8ch12ch(OTA_Packet8_s * const ota8, const uint32_t *channelData, bool const TelemetryStatus, bool const isHighAux)
{
    // All channel data is 10 bit apart from AUX1 which is 1 bit
    ota8->rc.packetType = PACKET_TYPE_RCDATA;
    ota8->rc.telemetryStatus = TelemetryStatus;
    // uplinkPower has 8 items but only 3 bits, but 0 is 0 power which we never use, shift 1-8 -> 0-7
    ota8->rc.uplinkPower = constrain(CRSF::LinkStatistics.uplink_TX_Power, 1, 8) - 1;
    ota8->rc.isHighAux = isHighAux;
    ota8->rc.ch4 = CRSF_to_BIT(channelData[4]);
#if defined(DEBUG_RCVR_LINKSTATS)
    // Incremental packet counter for verification on the RX side, 32 bits shoved into CH1-CH4
    ota8->dbg_linkstats.packetNum = packetCnt++;
#else
    // Sources:
    // 8ch always: low=0 high=5
    // 12ch isHighAux=false: low=0 high=5
    // 12ch isHighAux=true:  low=0 high=9
    // 16ch isHighAux=false: low=0 high=4
    // 16ch isHighAux=true:  low=8 high=12
    uint8_t chSrcLow;
    uint8_t chSrcHigh;
    if (OtaSwitchModeCurrent == smHybridOr16ch)
    {
        // 16ch mode
        if (isHighAux)
        {
            chSrcLow = 8;
            chSrcHigh = 12;
        }
        else
        {
            chSrcLow = 0;
            chSrcHigh = 4;
        }
    }
    else
    {
        chSrcLow = 0;
        chSrcHigh = isHighAux ? 9 : 5;
    }
    PackUInt11ToChannels4x10(&channelData[chSrcLow], &ota8->rc.chLow, &Decimate11to10_Div2);
    PackUInt11ToChannels4x10(&channelData[chSrcHigh], &ota8->rc.chHigh, &Decimate11to10_Div2);
#endif
}

static void ICACHE_RAM_ATTR GenerateChannelData8ch(OTA_Packet_s * const otaPktPtr, const uint32_t *channelData, bool const TelemetryStatus, uint8_t const tlmDenom)
{
    (void)tlmDenom;

    GenerateChannelData8ch12ch((OTA_Packet8_s * const)otaPktPtr, channelData, TelemetryStatus, false);
}

static bool FullResIsHighAux;
#if defined(UNIT_TEST)
void OtaSetFullResNextChannelSet(bool next) { FullResIsHighAux = next; }
#endif
static void ICACHE_RAM_ATTR GenerateChannelData12ch(OTA_Packet_s * const otaPktPtr, const uint32_t *channelData, bool const TelemetryStatus, uint8_t const tlmDenom)
{
    (void)tlmDenom;

    // Every time this function is called, the opposite high Aux channels are sent
    // This tries to ensure a fair split of high and low aux channels packets even
    // at 1:2 ratio and around sync packets
    GenerateChannelData8ch12ch((OTA_Packet8_s * const)otaPktPtr, channelData, TelemetryStatus, FullResIsHighAux);
    FullResIsHighAux = !FullResIsHighAux;
}
#endif


#if TARGET_RX || defined(UNIT_TEST)

// Current ChannelData unpacker function being used by RX
UnpackChannelData_t OtaUnpackChannelData;

#if defined(DEBUG_RCVR_LINKSTATS)
// Sequential PacketID from the TX
uint32_t debugRcvrLinkstatsPacketId;
#else

static void UnpackChannels4x10ToUInt11(OTA_Channels_4x10 const * const srcChannels4x10, uint32_t * const dest)
{
    uint8_t const * const payload = (uint8_t const * const)srcChannels4x10;
    constexpr unsigned numOfChannels = 4;
    constexpr unsigned srcBits = 10;
    constexpr unsigned dstBits = 11;
    constexpr unsigned inputChannelMask = (1 << srcBits) - 1;
    constexpr unsigned precisionShift = dstBits - srcBits;

    // code from BetaFlight rx/crsf.cpp / bitpacker_unpack
    uint8_t bitsMerged = 0;
    uint32_t readValue = 0;
    unsigned readByteIndex = 0;
    for (uint8_t n = 0; n < numOfChannels; n++)
    {
        while (bitsMerged < srcBits)
        {
            uint8_t readByte = payload[readByteIndex++];
            readValue |= ((uint32_t) readByte) << bitsMerged;
            bitsMerged += 8;
        }
        //printf("rv=%x(%x) bm=%u\n", readValue, (readValue & channelMask), bitsMerged);
        dest[n] = (readValue & inputChannelMask) << precisionShift;
        readValue >>= srcBits;
        bitsMerged -= srcBits;
    }
}
#endif /* !DEBUG_RCVR_LINKSTATS */

static void ICACHE_RAM_ATTR UnpackChannelDataHybridCommon(OTA_Packet4_s const * const ota4, uint32_t *channelData)
{
#if defined(DEBUG_RCVR_LINKSTATS)
    debugRcvrLinkstatsPacketId = ota4->dbg_linkstats.packetNum;
#else
    // The analog channels, encoded as 10bit where 0 = 998us and 1023 = 2012us
    UnpackChannels4x10ToUInt11(&ota4->rc.ch, &channelData[0]);
    // The unpacker simply does a << 1 to convert 10 to 11bit, but Hybrid/Wide modes
    // only pack a subset of the full range CRSF data, so properly expand it
    // This is ~80 bytes less code than passing an 10-to-11 expander fn to the unpacker
    for (unsigned ch=0; ch<4; ++ch)
    {
        channelData[ch] = UINT10_to_CRSF(channelData[ch] >> 1);
    }
    channelData[4] = BIT_to_CRSF(ota4->rc.ch4);
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
 * Output: channelData
 * Returns: TelemetryStatus bit
 */
bool ICACHE_RAM_ATTR UnpackChannelDataHybridSwitch8(OTA_Packet_s const * const otaPktPtr, uint32_t *channelData,
                                                    uint8_t const tlmDenom)
{
    (void)tlmDenom;

    OTA_Packet4_s const * const ota4 = (OTA_Packet4_s const * const)otaPktPtr;
    UnpackChannelDataHybridCommon(ota4, channelData);

    // The round-robin switch, switchIndex is actually index-1
    // to leave the low bit open for switch 7 (sent as 0b11x)
    // where x is the high bit of switch 7
    const uint8_t switchByte = ota4->rc.switches;
    uint8_t switchIndex = (switchByte & 0b111000) >> 3;
    if (switchIndex >= 6)
    {
        // Because AUX1 (index 0) is the low latency switch, the low bit
        // of the switchIndex can be used as data, and arrives as index "6"
        channelData[11] = N_to_CRSF(switchByte & 0b1111, 15);
    }
    else
    {
        channelData[5+switchIndex] = SWITCH3b_to_CRSF(switchByte & 0b111);
    }

    // TelemetryStatus bit
    return switchByte & (1 << 6);
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
 * Output: channelData
 * Returns: TelemetryStatus bit
 */
bool ICACHE_RAM_ATTR UnpackChannelDataHybridWide(OTA_Packet_s const * const otaPktPtr, uint32_t *channelData,
                                                 uint8_t const tlmDenom)
{
    static bool TelemetryStatus = false;

    OTA_Packet4_s const * const ota4 = (OTA_Packet4_s const * const)otaPktPtr;
    UnpackChannelDataHybridCommon(ota4, channelData);

    // The round-robin switch, 6-7 bits with the switch index implied by the nonce
    const uint8_t switchByte = ota4->rc.switches;
    bool telemInEveryPacket = (tlmDenom < 8);
    uint8_t switchIndex = HybridWideNonceToSwitchIndex(OtaNonce);
    if (telemInEveryPacket || switchIndex == 7)
          TelemetryStatus = (switchByte & 0b01000000) >> 6;
    if (switchIndex == 7)
    {
        CRSF::LinkStatistics.uplink_TX_Power = switchByte & 0b111111;
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

        channelData[5 + switchIndex] = N_to_CRSF(switchValue, bins);
    }

    return TelemetryStatus;
}

bool ICACHE_RAM_ATTR UnpackChannelData8ch(OTA_Packet_s const * const otaPktPtr, uint32_t *channelData, uint8_t const tlmDenom)
{
    (void)tlmDenom;

    OTA_Packet8_s const * const ota8 = (OTA_Packet8_s const * const)otaPktPtr;

#if defined(DEBUG_RCVR_LINKSTATS)
    debugRcvrLinkstatsPacketId = ota8->dbg_linkstats.packetNum;
#else
    uint8_t chDstLow;
    uint8_t chDstHigh;
    if (OtaSwitchModeCurrent == smHybridOr16ch)
    {
        if (ota8->rc.isHighAux)
        {
            chDstLow = 8;
            chDstHigh = 12;
        }
        else
        {
            chDstLow = 0;
            chDstHigh = 4;
        }
    }
    else
    {
        channelData[4] = BIT_to_CRSF(ota8->rc.ch4);
        chDstLow = 0;
        chDstHigh = (ota8->rc.isHighAux) ? 9 : 5;
    }

    // Analog channels packed 10bit covering the entire CRSF extended range (i.e. not just 988-2012)
    // ** Different than the 10bit encoding in Hybrid/Wide mode **
    UnpackChannels4x10ToUInt11(&ota8->rc.chLow, &channelData[chDstLow]);
    UnpackChannels4x10ToUInt11(&ota8->rc.chHigh, &channelData[chDstHigh]);
#endif
    // Restore the uplink_TX_Power range 0-7 -> 1-8
    CRSF::LinkStatistics.uplink_TX_Power = ota8->rc.uplinkPower + 1;
    return ota8->rc.telemetryStatus;
}
#endif

bool ICACHE_RAM_ATTR ValidatePacketCrcFull(OTA_Packet_s * const otaPktPtr)
{
    uint16_t const calculatedCRC =
        ota_crc.calc((uint8_t*)otaPktPtr, OTA8_CRC_CALC_LEN, OtaCrcInitializer);
    return otaPktPtr->full.crc == calculatedCRC;
}

bool ICACHE_RAM_ATTR ValidatePacketCrcStd(OTA_Packet_s * const otaPktPtr)
{
    uint16_t const inCRC = ((uint16_t)otaPktPtr->std.crcHigh << 8) + otaPktPtr->std.crcLow;
    // For smHybrid the CRC only has the packet type in byte 0
    // For smWide the FHSS slot is added to the CRC in byte 0 on PACKET_TYPE_RCDATAs
#if defined(TARGET_RX)
    if (otaPktPtr->std.type == PACKET_TYPE_RCDATA && OtaSwitchModeCurrent == smWideOr8ch)
    {
        otaPktPtr->std.crcHigh = (OtaNonce % ExpressLRS_currAirRate_Modparams->FHSShopInterval) + 1;
    }
    else
#endif
    {
        otaPktPtr->std.crcHigh = 0;
    }
    uint16_t const calculatedCRC =
        ota_crc.calc((uint8_t*)otaPktPtr, OTA4_CRC_CALC_LEN, OtaCrcInitializer);
    return inCRC == calculatedCRC;
}

void ICACHE_RAM_ATTR GeneratePacketCrcFull(OTA_Packet_s * const otaPktPtr)
{
    otaPktPtr->full.crc = ota_crc.calc((uint8_t*)otaPktPtr, OTA8_CRC_CALC_LEN, OtaCrcInitializer);
}

void ICACHE_RAM_ATTR GeneratePacketCrcStd(OTA_Packet_s * const otaPktPtr)
{
#if defined(TARGET_TX)
    // artificially inject the low bits of the nonce on data packets, this will be overwritten with the CRC after it's calculated
    if (otaPktPtr->std.type == PACKET_TYPE_RCDATA && OtaSwitchModeCurrent == smWideOr8ch)
    {
        otaPktPtr->std.crcHigh = (OtaNonce % ExpressLRS_currAirRate_Modparams->FHSShopInterval) + 1;
    }
#endif
    uint16_t crc = ota_crc.calc((uint8_t*)otaPktPtr, OTA4_CRC_CALC_LEN, OtaCrcInitializer);
    otaPktPtr->std.crcHigh = (crc >> 8);
    otaPktPtr->std.crcLow  = crc;
}

void OtaUpdateSerializers(OtaSwitchMode_e const switchMode, uint8_t packetSize)
{
    OtaIsFullRes = (packetSize == OTA8_PACKET_SIZE);

    if (OtaIsFullRes)
    {
        OtaValidatePacketCrc = &ValidatePacketCrcFull;
        OtaGeneratePacketCrc = &GeneratePacketCrcFull;
        ota_crc.init(16, ELRS_CRC16_POLY);

        #if defined(TARGET_TX) || defined(UNIT_TEST)
        if (switchMode == smWideOr8ch)
            OtaPackChannelData = &GenerateChannelData8ch;
        else
            OtaPackChannelData = &GenerateChannelData12ch;
        #endif
        #if defined(TARGET_RX) || defined(UNIT_TEST)
        OtaUnpackChannelData = &UnpackChannelData8ch;
        #endif
    } // is8ch

    else
    {
        OtaValidatePacketCrc = &ValidatePacketCrcStd;
        OtaGeneratePacketCrc = &GeneratePacketCrcStd;
        ota_crc.init(14, ELRS_CRC14_POLY);

        if (switchMode == smWideOr8ch)
        {
            #if defined(TARGET_TX) || defined(UNIT_TEST)
            OtaPackChannelData = &GenerateChannelDataHybridWide;
            #endif
            #if defined(TARGET_RX) || defined(UNIT_TEST)
            OtaUnpackChannelData = &UnpackChannelDataHybridWide;
            #endif
        } // !is8ch and smWideOr8ch

        else
        {
            #if defined(TARGET_TX) || defined(UNIT_TEST)
            OtaPackChannelData = &GenerateChannelDataHybrid8;
            #endif
            #if defined(TARGET_RX) || defined(UNIT_TEST)
            OtaUnpackChannelData = &UnpackChannelDataHybridSwitch8;
            #endif
        } // !is8ch and smHybridOr16ch
    }

    OtaSwitchModeCurrent = switchMode;
}

void OtaPackAirportData(OTA_Packet_s * const otaPktPtr, FIFO_GENERIC<AP_MAX_BUF_LEN>  * inputBuffer)
{
    uint8_t count = inputBuffer->size();
    if (OtaIsFullRes)
    {
        count = std::min(count, (uint8_t)ELRS8_TELEMETRY_BYTES_PER_CALL);
        otaPktPtr->full.airport.count = count;
        inputBuffer->popBytes(otaPktPtr->full.airport.payload, count);
    }
    else
    {
        count = std::min(count, (uint8_t)ELRS4_TELEMETRY_BYTES_PER_CALL);
        otaPktPtr->std.airport.count = count;
        inputBuffer->popBytes(otaPktPtr->std.airport.payload, count);
        otaPktPtr->std.airport.type = ELRS_TELEMETRY_TYPE_DATA;
    }
}

void OtaUnpackAirportData(OTA_Packet_s const * const otaPktPtr, FIFO_GENERIC<AP_MAX_BUF_LEN>  * outputBuffer)
{
    if (OtaIsFullRes)
    {
        uint8_t count = otaPktPtr->full.airport.count;
        outputBuffer->pushBytes(otaPktPtr->full.airport.payload, count);
    }
    else
    {
        uint8_t count = otaPktPtr->std.airport.count;
        outputBuffer->pushBytes(otaPktPtr->std.airport.payload, count);
    }
}
