/**
 * This file is part of ExpressLRS
 * See https://github.com/AlessandroAU/ExpressLRS
 *
 * Unit tests for over the air packet encoding, decoding and associated utilities
 *
 * Entry point is setup_switches()
 */


#include <unity.h>

#include "targets.h"
#include "common.h"
#include "CRSF.h"
#include "POWERMGNT.h"
#include <OTA.h>
#include "crsf_sysmocks.h"

CRSF crsf;  // need an instance to provide the fields used by the code under test
uint32_t ChannelData[CRSF_NUM_CHANNELS];      // Current state of channels, CRSF format
uint8_t UID[6] = {1,2,3,4,5,6};

void test_crsf_endpoints()
{
    // Validate 988us and 2012us convert to approprate CRSF values. Spoiler: They don't
    TEST_ASSERT_EQUAL(-1024, Us_to_OpenTx(988));
    TEST_ASSERT_EQUAL(CRSF_CHANNEL_VALUE_MIN+1, OpenTx_to_Crsf(-1024)); // NOTE: 988 comes from OpenTX as 173, not 172!
    TEST_ASSERT_EQUAL(1024, Us_to_OpenTx(2012));
    TEST_ASSERT_EQUAL(CRSF_CHANNEL_VALUE_MAX, OpenTx_to_Crsf(1024));

    // Validate CRSF values convert to their expected values in OpenTX
    TEST_ASSERT_EQUAL(988, Crsf_to_OpenTx_to_Us(CRSF_CHANNEL_VALUE_MIN));
    TEST_ASSERT_EQUAL(2012-1, Crsf_to_OpenTx_to_Us(CRSF_CHANNEL_VALUE_MAX)); // NOTE: Feeding the 2012 CRSF value back into OpenTX would give 2011

    // Validate CRSF values convert to their expected values in Betaflight
    TEST_ASSERT_EQUAL(988, Crsf_to_BfUs(CRSF_CHANNEL_VALUE_MIN));
    TEST_ASSERT_EQUAL(2012, Crsf_to_BfUs(CRSF_CHANNEL_VALUE_MAX));

    // Validate important values are still the same value when mapped and umapped from their 10-bit representations
    TEST_ASSERT_EQUAL(CRSF_CHANNEL_VALUE_MIN,  Crsf_to_Uint10_to_Crsf(CRSF_CHANNEL_VALUE_MIN));
    TEST_ASSERT_EQUAL(CRSF_CHANNEL_VALUE_1000, Crsf_to_Uint10_to_Crsf(CRSF_CHANNEL_VALUE_1000));
    TEST_ASSERT_EQUAL(CRSF_CHANNEL_VALUE_MID,  Crsf_to_Uint10_to_Crsf(CRSF_CHANNEL_VALUE_MID));
    TEST_ASSERT_EQUAL(CRSF_CHANNEL_VALUE_2000, Crsf_to_Uint10_to_Crsf(CRSF_CHANNEL_VALUE_2000));
    TEST_ASSERT_EQUAL(CRSF_CHANNEL_VALUE_MAX,  Crsf_to_Uint10_to_Crsf(CRSF_CHANNEL_VALUE_MAX));
}

void test_crsfToBit()
{
    TEST_ASSERT_EQUAL(0, CRSF_to_BIT(CRSF_CHANNEL_VALUE_1000));
    TEST_ASSERT_EQUAL(1, CRSF_to_BIT(CRSF_CHANNEL_VALUE_2000));
}

void test_bitToCrsf()
{
    TEST_ASSERT_EQUAL(CRSF_CHANNEL_VALUE_1000, BIT_to_CRSF(0));
    TEST_ASSERT_EQUAL(CRSF_CHANNEL_VALUE_2000, BIT_to_CRSF(1));
}

void test_crsfToN()
{
    TEST_ASSERT_EQUAL(0, CRSF_to_N(CRSF_CHANNEL_VALUE_MIN, 64));
    TEST_ASSERT_EQUAL(0, CRSF_to_N(CRSF_CHANNEL_VALUE_1000, 64));
    TEST_ASSERT_EQUAL(0b100000, CRSF_to_N(CRSF_CHANNEL_VALUE_MID, 64));
    TEST_ASSERT_EQUAL(0b111111, CRSF_to_N(CRSF_CHANNEL_VALUE_2000, 64));
    TEST_ASSERT_EQUAL(0b111111, CRSF_to_N(CRSF_CHANNEL_VALUE_MAX, 64));

    TEST_ASSERT_EQUAL(0, CRSF_to_N(CRSF_CHANNEL_VALUE_MIN, 128));
    TEST_ASSERT_EQUAL(0, CRSF_to_N(CRSF_CHANNEL_VALUE_1000, 128));
    TEST_ASSERT_EQUAL(0b1000000, CRSF_to_N(CRSF_CHANNEL_VALUE_MID, 128));
    TEST_ASSERT_EQUAL(0b1111111, CRSF_to_N(CRSF_CHANNEL_VALUE_2000, 128));
    TEST_ASSERT_EQUAL(0b1111111, CRSF_to_N(CRSF_CHANNEL_VALUE_MAX, 128));
}

void test_nToCrsf()
{
    // 6-bit
    TEST_ASSERT_EQUAL(CRSF_CHANNEL_VALUE_1000, N_to_CRSF(0, 63));
    TEST_ASSERT_EQUAL(1004, N_to_CRSF(0b100000, 63));
    TEST_ASSERT_EQUAL(CRSF_CHANNEL_VALUE_2000, N_to_CRSF(0b111111, 63));

    // 7-bit
    TEST_ASSERT_EQUAL(CRSF_CHANNEL_VALUE_1000, N_to_CRSF(0, 127));
    TEST_ASSERT_EQUAL(997, N_to_CRSF(0b1000000, 127));
    TEST_ASSERT_EQUAL(CRSF_CHANNEL_VALUE_2000, N_to_CRSF(0b1111111, 127));
}

// ------------------------------------------------
// Test the hybrid8 encoding/decoding

/* Check the hybrid 8 encoding of a packet for OTA tx
*/
void test_encodingHybrid8(bool highResChannel)
{
    constexpr uint8_t N_SWITCHES = 8;
    uint8_t UID[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE};
    uint8_t TXdataBuffer[OTA4_PACKET_SIZE] = {0};
    OTA_Packet_s * const otaPktPtr = (OTA_Packet_s *)TXdataBuffer;

    // Define the input data
    // 4 channels of 11-bit analog data
    ChannelData[0] = 0x0123 & 0b11111111111;
    ChannelData[1] = 0x4567 & 0b11111111111;
    ChannelData[2] = 0x89AB & 0b11111111111;
    ChannelData[3] = 0xCDEF & 0b11111111111;

    OTA_Packet8_s ota;
    OTA_Channels_4x10 ch;
    switch (OtaNonce)
    {
        case sizeof(ota.rc):
        break;
        case 13:
        break;
        case 17:
        break;
        case sizeof(ch):
        break;
        case 8:
        break;
    }
    // 8 switches
    for(int i = 0; i < N_SWITCHES; i++) {
        constexpr int CHANNELS[] =
            { CRSF_CHANNEL_VALUE_1000, CRSF_CHANNEL_VALUE_MID, CRSF_CHANNEL_VALUE_2000 };
        ChannelData[4+i] = CHANNELS[i % 3];
    }

    // set the nextSwitchIndex so we know which switch to expect in the packet
    if (highResChannel)
        OtaSetHybrid8NextSwitchIndex(7-1);
    else
        OtaSetHybrid8NextSwitchIndex(3-1);

    // encode it
    OtaUpdateSerializers(smHybridOr16ch, OTA4_PACKET_SIZE);
    OtaPackChannelData(otaPktPtr, ChannelData, false, 0);

    // check it looks right
    // 1st byte is CRC & packet type
    uint8_t header = PACKET_TYPE_RCDATA;
    TEST_ASSERT_EQUAL(header, TXdataBuffer[0]);

    // bytes 1 through 5 are 10 bit packed analog channels representing 998-2012 (CRSF_CHANNEL_VALUE_MIN-CRSF_CHANNEL_VALUE_MAX)
    uint8_t expected[5] = { 0x4a, 0xd0, 0xfb, 0x49, 0xd2 };
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, &TXdataBuffer[1], 5);

    // byte 6 is the switch encoding
    TEST_ASSERT_EQUAL(CRSF_to_BIT(ChannelData[4+0]), TXdataBuffer[6] >> 7);
    // top bit is undefined
    // expect switch 0 in bit 6
    // index-1 in 3-5
    // value in 0,1,2[,3]
    if (highResChannel)
    {
        TEST_ASSERT_EQUAL(7, ((TXdataBuffer[6] & 0b110000)>>3) + 1);
        TEST_ASSERT_EQUAL(CRSF_to_N(ChannelData[4+7], 16), TXdataBuffer[6] & 0b1111);
    }
    else
    {
        TEST_ASSERT_EQUAL(3, ((TXdataBuffer[6] & 0b111000)>>3) + 1);
        TEST_ASSERT_EQUAL(CRSF_to_N(ChannelData[4+3], 6), TXdataBuffer[6] & 0b0111);
    }
}

void test_encodingHybrid8_3()
{
    test_encodingHybrid8(false);
}

void test_encodingHybrid8_7()
{
    test_encodingHybrid8(true);
}

/* Check the decoding of a packet after rx
*/
void test_decodingHybrid8(uint8_t forceSwitch, uint8_t switchval)
{
    constexpr uint8_t N_SWITCHES = 8;
    uint8_t UID[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE};
    uint8_t TXdataBuffer[OTA4_PACKET_SIZE] = {0};
    OTA_Packet_s * const otaPktPtr = (OTA_Packet_s *)TXdataBuffer;
    uint32_t ChannelsIn[16];
    TEST_ASSERT_EQUAL(sizeof(ChannelData), sizeof(ChannelsIn));

    // Define the input data
    // 4 channels of 11-bit analog data
    ChannelData[0] = 0x0123 & 0b11111111111;
    ChannelData[1] = 0x4567 & 0b11111111111;
    ChannelData[2] = 0x89AB & 0b11111111111;
    ChannelData[3] = 0xCDEF & 0b11111111111;

    // 8 switches
    for(int i = 0; i < N_SWITCHES; i++) {
        constexpr int CHANNELS[] =
            { CRSF_CHANNEL_VALUE_1000, CRSF_CHANNEL_VALUE_MID, CRSF_CHANNEL_VALUE_2000 };
        ChannelData[4+i] = CHANNELS[i % 3];
    }
    if (forceSwitch == 0)
        ChannelData[4+forceSwitch] = BIT_to_CRSF(switchval);
    else if (forceSwitch == 7)
        ChannelData[4+forceSwitch] = N_to_CRSF(switchval, 15);
    else
        ChannelData[4+forceSwitch] = SWITCH3b_to_CRSF(switchval);

    // set the nextSwitchIndex so we know which switch to expect in the packet
    if (forceSwitch == 0)
        OtaSetHybrid8NextSwitchIndex(0);
    else
        OtaSetHybrid8NextSwitchIndex(forceSwitch-1);

    // Save the channels since they go into the same place
    memcpy(ChannelsIn, ChannelData, sizeof(ChannelData));
    // use the encoding method to pack it into TXdataBuffer
    OtaUpdateSerializers(smHybridOr16ch, OTA4_PACKET_SIZE);
    OtaPackChannelData(otaPktPtr, ChannelData, false, 0);

    // run the decoder, results in crsf->PackedRCdataOut
    OtaUnpackChannelData(otaPktPtr, ChannelData, 0);

    // compare the unpacked results with the input data
    TEST_ASSERT_EQUAL(ChannelsIn[0], ChannelData[0]);
    TEST_ASSERT_EQUAL(ChannelsIn[1], ChannelData[1]);
    TEST_ASSERT_EQUAL(ChannelsIn[2], ChannelData[2]);
    TEST_ASSERT_EQUAL(ChannelsIn[3], ChannelData[3]);

    TEST_ASSERT_EQUAL(ChannelsIn[4+0], ChannelData[4]); // Switch 0 is sent on every packet
    if (forceSwitch == 7)
        TEST_ASSERT_EQUAL(ChannelsIn[4+forceSwitch], ChannelData[11]); // We forced switch 1 to be sent as the sequential field
    else if (forceSwitch != 0)
        TEST_ASSERT_EQUAL(ChannelData[4+forceSwitch], ChannelData[4+forceSwitch]);
}

void test_decodingHybrid8_all()
{
    // Switch 0 is 2 pos
    test_decodingHybrid8(0, 0);
    test_decodingHybrid8(0, 1);
    // Switch X in 6-pos mode (includes 3-pos low/high)
    for (uint8_t val=0; val<6; ++val)
        test_decodingHybrid8(3, val);
    // // Switch X in 3-pos mode center
    // test_decodingHybrid8(3, 7);
    // // Switch 7 is 16 pos
    // for (uint8_t val=0; val<16; ++val)
    //     test_decodingHybrid8(7, val);
}

/* Check the HybridWide encoding of a packet for OTA tx
*/
void test_encodingHybridWide(bool highRes, uint8_t nonce)
{
    uint8_t UID[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE};
    uint8_t TXdataBuffer[OTA4_PACKET_SIZE] = {0};
    OTA_Packet_s * const otaPktPtr = (OTA_Packet_s *)TXdataBuffer;
    uint32_t ChannelsIn[16];
    TEST_ASSERT_EQUAL(sizeof(ChannelData), sizeof(ChannelsIn));

    // Define the input data
    // 4 channels of 11-bit analog data
    ChannelData[0] = 0x0123 & 0b11111111111;
    ChannelData[1] = 0x4567 & 0b11111111111;
    ChannelData[2] = 0x89AB & 0b11111111111;
    ChannelData[3] = 0xCDEF & 0b11111111111;

    // 8 switches
    constexpr int N_SWITCHES = 8;
    for(int i = 0; i < N_SWITCHES; i++) {
        constexpr unsigned CHANNELS[] =
            { CRSF_CHANNEL_VALUE_1000, CRSF_CHANNEL_VALUE_MID, CRSF_CHANNEL_VALUE_2000 };
        ChannelData[4+i] = CHANNELS[i % 3];
    }

    // Uplink data
    crsf.LinkStatistics.uplink_TX_Power = 3; // 100mW

    // Save the channels since they go into the same place
    memcpy(ChannelsIn, ChannelData, sizeof(ChannelData));
    // encode it
    uint8_t tlmDenom = (highRes) ? 64 : 4;
    OtaUpdateSerializers(smWideOr8ch, OTA4_PACKET_SIZE);
    OtaNonce = nonce;
    OtaPackChannelData(otaPktPtr, ChannelData, nonce % 2, tlmDenom);

    // check it looks right
    // 1st byte is CRC & packet type
    uint8_t header = PACKET_TYPE_RCDATA;
    TEST_ASSERT_EQUAL(header, TXdataBuffer[0]);

    // bytes 1 through 5 are 10 bit packed analog channels representing 998-2012 (CRSF_CHANNEL_VALUE_MIN-CRSF_CHANNEL_VALUE_MAX)
    uint8_t expected[5] = { 0x4a, 0xd0, 0xfb, 0x49, 0xd2 };
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, &TXdataBuffer[1], 5);

    // byte 6 is the switches encoded
    uint8_t switches = TXdataBuffer[6];
    uint8_t switchIdx = nonce % 8;

    // High bit should be AUX1
    TEST_ASSERT_EQUAL(CRSF_to_BIT(ChannelsIn[4]), switches >> 7);
    // If low res or slot 7, the bit 6 should be the telemetryack bit
    if (!highRes || switchIdx == 7)
        TEST_ASSERT_EQUAL(nonce % 2, (switches >> 6) & 1);

    // If slot 7, the uplink_TX_Power should be in the low 6 bits
    if (switchIdx == 7)
        TEST_ASSERT_EQUAL(crsf.LinkStatistics.uplink_TX_Power, switches & 0b111111);
    else
    {
        uint16_t ch = ChannelData[5+switchIdx];
        if (highRes)
            TEST_ASSERT_EQUAL(CRSF_to_N(ch, 128), switches & 0b1111111); // 7-bit
        else
            TEST_ASSERT_EQUAL(CRSF_to_N(ch, 64), switches & 0b111111); // 6-bit
    }
}

void test_encodingHybridWide_high()
{
    constexpr int N_SWITCHES = 8;
    for (int i=0; i<N_SWITCHES; ++i)
        test_encodingHybridWide(true, i);
}

void test_encodingHybridWide_low()
{
    constexpr int N_SWITCHES = 8;
    for (int i=0; i<N_SWITCHES; ++i)
        test_encodingHybridWide(false, i);
}

/* Check the decoding of a packet after rx in HybridWide mode
*/
void test_decodingHybridWide(bool highRes, uint8_t nonce, uint8_t forceSwitch, uint16_t forceVal)
{
    uint8_t UID[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE};
    uint8_t TXdataBuffer[OTA4_PACKET_SIZE] = {0};
    OTA_Packet_s * const otaPktPtr = (OTA_Packet_s *)TXdataBuffer;
    uint32_t ChannelsIn[16];
    TEST_ASSERT_EQUAL(sizeof(ChannelData), sizeof(ChannelsIn));

    // Define the input data
    // 4 channels of 11-bit analog data
    ChannelData[0] = 0x0123 & 0b11111111111;
    ChannelData[1] = 0x4567 & 0b11111111111;
    ChannelData[2] = 0x89AB & 0b11111111111;
    ChannelData[3] = 0xCDEF & 0b11111111111;

    // 8 switches
    constexpr int N_SWITCHES = 8;
    for(int i = 0; i < N_SWITCHES; i++) {
        constexpr unsigned CHANNELS[] =
            { CRSF_CHANNEL_VALUE_1000, CRSF_CHANNEL_VALUE_MID, CRSF_CHANNEL_VALUE_2000 };
        if (i == forceSwitch)
            ChannelData[4+i] = forceVal;
        else
            ChannelData[4+i] = CHANNELS[i % 3];
    }

    // Uplink data
    crsf.LinkStatistics.uplink_TX_Power = 3; // 100mW

    // Save the channels since they go into the same place
    memcpy(ChannelsIn, ChannelData, sizeof(ChannelData));
    // encode it
    uint8_t tlmDenom = (highRes) ? 64 : 4;
    OtaUpdateSerializers(smWideOr8ch, OTA4_PACKET_SIZE);
    OtaNonce = nonce;
    OtaPackChannelData(otaPktPtr, ChannelData, nonce % 2, tlmDenom);

    // Clear the LinkStatistics to receive it from the encoding
    crsf.LinkStatistics.uplink_TX_Power = 0;

    // run the decoder, results in crsf->PackedRCdataOut
    bool telemResult = OtaUnpackChannelData(otaPktPtr, ChannelData, tlmDenom);

    // compare the unpacked results with the input data
    TEST_ASSERT_EQUAL(ChannelsIn[0], ChannelData[0]);
    TEST_ASSERT_EQUAL(ChannelsIn[1], ChannelData[1]);
    TEST_ASSERT_EQUAL(ChannelsIn[2], ChannelData[2]);
    TEST_ASSERT_EQUAL(ChannelsIn[3], ChannelData[3]);

    // Switch 0 is sent on every packet
    TEST_ASSERT_EQUAL(ChannelData[4], ChannelData[4]);

    uint8_t switchIdx = nonce % 8;
    // Validate the telemResult was unpacked properly
    if (!highRes || switchIdx == 7)
        TEST_ASSERT_EQUAL(telemResult, nonce % 2);

    if (switchIdx == 7)
    {
        TEST_ASSERT_EQUAL(crsf.LinkStatistics.uplink_TX_Power, 3);
    }
    else
    {
        if (highRes)
            TEST_ASSERT_EQUAL(N_to_CRSF(CRSF_to_N(ChannelData[5+switchIdx], 128), 127), ChannelData[5+switchIdx]);
        else
            TEST_ASSERT_EQUAL(N_to_CRSF(CRSF_to_N(ChannelData[5+switchIdx], 64), 63), ChannelData[5+switchIdx]);
    }
}

void fullres_fillChannelData()
{
    // Define the input data
    // 16 channels of 11-bit analog data
    ChannelData[0] = 0x0123 & 0b11111111111;
    ChannelData[1] = 0x4567 & 0b11111111111;
    ChannelData[2] = 0x89AB & 0b11111111111;
    ChannelData[3] = 0xCDEF & 0b11111111111;
    ChannelData[4] = 0x3210 & 0b11111111111;
    ChannelData[5] = 0x7654 & 0b11111111111;
    ChannelData[6] = 0xBA98 & 0b11111111111;
    ChannelData[7] = 0xFEDC & 0b11111111111;

    ChannelData[8]  = 0x2301 & 0b11111111111;
    ChannelData[9]  = 0x6745 & 0b11111111111;
    ChannelData[10] = 0xAB89 & 0b11111111111;
    ChannelData[11] = 0xEFCD & 0b11111111111;
    ChannelData[12] = 0x1023 & 0b11111111111;
    ChannelData[13] = 0x5476 & 0b11111111111;
    ChannelData[14] = 0x98BA & 0b11111111111;
    ChannelData[15] = 0xDCFE & 0b11111111111;
}

void test_encodingFullresPowerLevels()
{
    uint8_t TXdataBuffer[OTA8_PACKET_SIZE] = {0};
    OTA_Packet_s * const otaPktPtr = (OTA_Packet_s *)TXdataBuffer;
    uint32_t ChannelsIn[16];
    TEST_ASSERT_EQUAL(sizeof(ChannelData), sizeof(ChannelsIn));

    OtaUpdateSerializers(smWideOr8ch, OTA8_PACKET_SIZE);

    for (uint8_t pwr=PWR_10mW; pwr<PWR_COUNT; ++pwr)
    {
        memset(TXdataBuffer, 0, sizeof(TXdataBuffer));
        fullres_fillChannelData();

        // This is what we're testing here, just the power
        uint8_t crsfPower = powerToCrsfPower((PowerLevels_e)pwr);
        crsf.LinkStatistics.uplink_TX_Power = crsfPower;

        OtaPackChannelData(otaPktPtr, ChannelData, false, 0);
        OtaUnpackChannelData(otaPktPtr, ChannelData, 0);

        TEST_ASSERT_EQUAL(crsfPower, crsf.LinkStatistics.uplink_TX_Power);
    }
}

void test_encodingFullres8ch()
{
    uint8_t TXdataBuffer[OTA8_PACKET_SIZE] = {0};
    OTA_Packet_s * const otaPktPtr = (OTA_Packet_s *)TXdataBuffer;
    uint32_t ChannelsIn[16];
    TEST_ASSERT_EQUAL(sizeof(ChannelData), sizeof(ChannelsIn));

    fullres_fillChannelData();
    crsf.LinkStatistics.uplink_TX_Power = PWR_250mW;

    // Save the channels since they go into the same place
    memcpy(ChannelsIn, ChannelData, sizeof(ChannelData));
    OtaUpdateSerializers(smWideOr8ch, OTA8_PACKET_SIZE);
    OtaPackChannelData(otaPktPtr, ChannelData, false, 0);

    // Low 4ch (CH1-CH4)
    uint8_t expected[5];
    expected[0] = ((ChannelsIn[0] >> 1) >> 0);
    expected[1] = ((ChannelsIn[0] >> 1) >> 8) | ((ChannelsIn[1] >> 1) << 2);
    expected[2] = ((ChannelsIn[1] >> 1) >> 6) | ((ChannelsIn[2] >> 1) << 4);
    expected[3] = ((ChannelsIn[2] >> 1) >> 4) | ((ChannelsIn[3] >> 1) << 6);
    expected[4] = ((ChannelsIn[3] >> 1) >> 2);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, &TXdataBuffer[offsetof(OTA_Packet8_s, rc.chLow)], 5);
    // High 4ch, skip AUX1 (CH6-CH9)
    expected[0] = ((ChannelsIn[5] >> 1) >> 0);
    expected[1] = ((ChannelsIn[5] >> 1) >> 8) | ((ChannelsIn[6] >> 1) << 2);
    expected[2] = ((ChannelsIn[6] >> 1) >> 6) | ((ChannelsIn[7] >> 1) << 4);
    expected[3] = ((ChannelsIn[7] >> 1) >> 4) | ((ChannelsIn[8] >> 1) << 6);
    expected[4] = ((ChannelsIn[8] >> 1) >> 2);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, &TXdataBuffer[offsetof(OTA_Packet8_s, rc.chHigh)], 5);

    // Check the header bits
    TEST_ASSERT_EQUAL(PACKET_TYPE_RCDATA, otaPktPtr->full.rc.packetType);
    TEST_ASSERT_EQUAL(false, otaPktPtr->full.rc.telemetryStatus);
    TEST_ASSERT_EQUAL(PWR_250mW, otaPktPtr->full.rc.uplinkPower + 1);
    TEST_ASSERT_EQUAL(false, otaPktPtr->full.rc.isHighAux);
    TEST_ASSERT_EQUAL(CRSF_to_BIT(ChannelsIn[4]), otaPktPtr->full.rc.ch4);
}

void test_encodingFullres16ch()
{
    uint8_t TXdataBuffer[OTA8_PACKET_SIZE] = {0};
    OTA_Packet_s * const otaPktPtr = (OTA_Packet_s *)TXdataBuffer;
    uint32_t ChannelsIn[16];
    TEST_ASSERT_EQUAL(sizeof(ChannelData), sizeof(ChannelsIn));

    fullres_fillChannelData();

    // Save the channels since they go into the same place
    memcpy(ChannelsIn, ChannelData, sizeof(ChannelData));
    OtaUpdateSerializers(smHybridOr16ch, OTA8_PACKET_SIZE);
    OtaSetFullResNextChannelSet(false);

    // ** PACKET ONE **
    memset(TXdataBuffer, 0, sizeof(TXdataBuffer)); // "destChannels4x10 must be zeroed"
    OtaPackChannelData(otaPktPtr, ChannelData, false, 0);
    // Low 4ch (CH1-CH4)
    uint8_t expected[5];
    expected[0] = ((ChannelsIn[0] >> 1) >> 0);
    expected[1] = ((ChannelsIn[0] >> 1) >> 8) | ((ChannelsIn[1] >> 1) << 2);
    expected[2] = ((ChannelsIn[1] >> 1) >> 6) | ((ChannelsIn[2] >> 1) << 4);
    expected[3] = ((ChannelsIn[2] >> 1) >> 4) | ((ChannelsIn[3] >> 1) << 6);
    expected[4] = ((ChannelsIn[3] >> 1) >> 2);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, &TXdataBuffer[offsetof(OTA_Packet8_s, rc.chLow)], 5);
    // High 4ch, includes AUX1 (CH5-CH8)
    expected[0] = ((ChannelsIn[4] >> 1) >> 0);
    expected[1] = ((ChannelsIn[4] >> 1) >> 8) | ((ChannelsIn[5] >> 1) << 2);
    expected[2] = ((ChannelsIn[5] >> 1) >> 6) | ((ChannelsIn[6] >> 1) << 4);
    expected[3] = ((ChannelsIn[6] >> 1) >> 4) | ((ChannelsIn[7] >> 1) << 6);
    expected[4] = ((ChannelsIn[7] >> 1) >> 2);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, &TXdataBuffer[offsetof(OTA_Packet8_s, rc.chHigh)], 5);

    // ** PACKET TWO **
    memset(TXdataBuffer, 0, sizeof(TXdataBuffer)); // "destChannels4x10 must be zeroed"
    OtaPackChannelData(otaPktPtr, ChannelData, false, 0);
    // Low 4ch (CH9-CH12)
    expected[0] = ((ChannelsIn[8] >> 1) >> 0);
    expected[1] = ((ChannelsIn[8] >> 1) >> 8) | ((ChannelsIn[9] >> 1) << 2);
    expected[2] = ((ChannelsIn[9] >> 1) >> 6) | ((ChannelsIn[10] >> 1) << 4);
    expected[3] = ((ChannelsIn[10] >> 1) >> 4) | ((ChannelsIn[11] >> 1) << 6);
    expected[4] = ((ChannelsIn[11] >> 1) >> 2);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, &TXdataBuffer[offsetof(OTA_Packet8_s, rc.chLow)], 5);
    // High 4ch (CH13-CH16)
    expected[0] = ((ChannelsIn[12] >> 1) >> 0);
    expected[1] = ((ChannelsIn[12] >> 1) >> 8) | ((ChannelsIn[13] >> 1) << 2);
    expected[2] = ((ChannelsIn[13] >> 1) >> 6) | ((ChannelsIn[14] >> 1) << 4);
    expected[3] = ((ChannelsIn[14] >> 1) >> 4) | ((ChannelsIn[15] >> 1) << 6);
    expected[4] = ((ChannelsIn[15] >> 1) >> 2);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, &TXdataBuffer[offsetof(OTA_Packet8_s, rc.chHigh)], 5);
}

void test_encodingFullres12ch()
{
    uint8_t TXdataBuffer[OTA8_PACKET_SIZE] = {0};
    OTA_Packet_s * const otaPktPtr = (OTA_Packet_s *)TXdataBuffer;
    uint32_t ChannelsIn[16];
    TEST_ASSERT_EQUAL(sizeof(ChannelData), sizeof(ChannelsIn));

    fullres_fillChannelData();

    // Save the channels since they go into the same place
    memcpy(ChannelsIn, ChannelData, sizeof(ChannelData));
    OtaUpdateSerializers(sm12ch, OTA8_PACKET_SIZE);
    OtaSetFullResNextChannelSet(false);

    // ** PACKET ONE **
    memset(TXdataBuffer, 0, sizeof(TXdataBuffer)); // "destChannels4x10 must be zeroed"
    OtaPackChannelData(otaPktPtr, ChannelData, false, 0);
    // Low 4ch (CH1-CH4)
    uint8_t expected[5];
    expected[0] = ((ChannelsIn[0] >> 1) >> 0);
    expected[1] = ((ChannelsIn[0] >> 1) >> 8) | ((ChannelsIn[1] >> 1) << 2);
    expected[2] = ((ChannelsIn[1] >> 1) >> 6) | ((ChannelsIn[2] >> 1) << 4);
    expected[3] = ((ChannelsIn[2] >> 1) >> 4) | ((ChannelsIn[3] >> 1) << 6);
    expected[4] = ((ChannelsIn[3] >> 1) >> 2);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, &TXdataBuffer[offsetof(OTA_Packet8_s, rc.chLow)], 5);
    // High 4ch, skips AUX1 (CH6-CH9)
    expected[0] = ((ChannelsIn[5] >> 1) >> 0);
    expected[1] = ((ChannelsIn[5] >> 1) >> 8) | ((ChannelsIn[6] >> 1) << 2);
    expected[2] = ((ChannelsIn[6] >> 1) >> 6) | ((ChannelsIn[7] >> 1) << 4);
    expected[3] = ((ChannelsIn[7] >> 1) >> 4) | ((ChannelsIn[8] >> 1) << 6);
    expected[4] = ((ChannelsIn[8] >> 1) >> 2);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, &TXdataBuffer[offsetof(OTA_Packet8_s, rc.chHigh)], 5);

    // ** PACKET TWO **
    memset(TXdataBuffer, 0, sizeof(TXdataBuffer)); // "destChannels4x10 must be zeroed"
    OtaPackChannelData(otaPktPtr, ChannelData, false, 0);
    // Low 4ch (CH1-CH4)
    expected[0] = ((ChannelsIn[0] >> 1) >> 0);
    expected[1] = ((ChannelsIn[0] >> 1) >> 8) | ((ChannelsIn[1] >> 1) << 2);
    expected[2] = ((ChannelsIn[1] >> 1) >> 6) | ((ChannelsIn[2] >> 1) << 4);
    expected[3] = ((ChannelsIn[2] >> 1) >> 4) | ((ChannelsIn[3] >> 1) << 6);
    expected[4] = ((ChannelsIn[3] >> 1) >> 2);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, &TXdataBuffer[offsetof(OTA_Packet8_s, rc.chLow)], 5);
    // Other high 4ch, skip AUX1 (CH10-CH13)
    expected[0] = ((ChannelsIn[9] >> 1) >> 0);
    expected[1] = ((ChannelsIn[9] >> 1) >> 8) | ((ChannelsIn[10] >> 1) << 2);
    expected[2] = ((ChannelsIn[10] >> 1) >> 6) | ((ChannelsIn[11] >> 1) << 4);
    expected[3] = ((ChannelsIn[11] >> 1) >> 4) | ((ChannelsIn[12] >> 1) << 6);
    expected[4] = ((ChannelsIn[12] >> 1) >> 2);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, &TXdataBuffer[offsetof(OTA_Packet8_s, rc.chHigh)], 5);
}

void test_decodingFullres16chLow()
{
    uint8_t TXdataBuffer[OTA8_PACKET_SIZE] = {0};
    OTA_Packet_s * const otaPktPtr = (OTA_Packet_s *)TXdataBuffer;
    uint32_t ChannelsIn[16];
    TEST_ASSERT_EQUAL(sizeof(ChannelData), sizeof(ChannelsIn));

    fullres_fillChannelData();

    // Save the channels since they go into the same place
    memcpy(ChannelsIn, ChannelData, sizeof(ChannelData));
    OtaUpdateSerializers(smHybridOr16ch, OTA8_PACKET_SIZE);
    OtaSetFullResNextChannelSet(false);

    // ** PACKET ONE **
    memset(TXdataBuffer, 0, sizeof(TXdataBuffer)); // "destChannels4x10 must be zeroed"
    OtaPackChannelData(otaPktPtr, ChannelData, false, 0);
    OtaUnpackChannelData(otaPktPtr, ChannelData, 0);
    for (unsigned ch=0; ch<8; ++ch)
    {
        TEST_ASSERT_EQUAL(ChannelsIn[ch] & 0b11111111110, ChannelData[ch]);
    }

    // ** PACKET TWO **
    memset(TXdataBuffer, 0, sizeof(TXdataBuffer)); // "destChannels4x10 must be zeroed"
    OtaPackChannelData(otaPktPtr, ChannelData, false, 0);
    OtaUnpackChannelData(otaPktPtr, ChannelData, 0);
    for (unsigned ch=9; ch<16; ++ch)
    {
        TEST_ASSERT_EQUAL(ChannelsIn[ch] & 0b11111111110, ChannelData[ch]);
    }
}

void test_decodingHybridWide_AUX1()
{
    // Switch 0 is 2 pos, also tests the uplink_TX_Power
    test_decodingHybridWide(true, 7, 0, CRSF_CHANNEL_VALUE_1000);
    test_decodingHybridWide(true, 7, 0, CRSF_CHANNEL_VALUE_2000);
}

void test_decodingHybridWide_AUXX_high()
{
    constexpr int N_SWITCHES = 8;
    for (int i=0; i<N_SWITCHES; ++i)
        test_decodingHybridWide(true, i, 0, CRSF_CHANNEL_VALUE_1000);
}

void test_decodingHybridWide_AUXX_low()
{
    constexpr int N_SWITCHES = 8;
    for (int i=0; i<N_SWITCHES; ++i)
        test_decodingHybridWide(false, i, 0, CRSF_CHANNEL_VALUE_1000);
}

// Unity setup/teardown
void setUp() {}
void tearDown() {}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_crsf_endpoints);
    RUN_TEST(test_crsfToBit);
    RUN_TEST(test_bitToCrsf);
    RUN_TEST(test_crsfToN);
    RUN_TEST(test_nToCrsf);

    RUN_TEST(test_encodingHybrid8_3);
    RUN_TEST(test_encodingHybrid8_7);
    RUN_TEST(test_decodingHybrid8_all);

    RUN_TEST(test_encodingHybridWide_high);
    RUN_TEST(test_encodingHybridWide_low);
    RUN_TEST(test_decodingHybridWide_AUX1);
    RUN_TEST(test_decodingHybridWide_AUXX_high);
    RUN_TEST(test_decodingHybridWide_AUXX_low);

    RUN_TEST(test_encodingFullresPowerLevels);
    RUN_TEST(test_encodingFullres8ch);
    RUN_TEST(test_encodingFullres16ch);
    RUN_TEST(test_encodingFullres12ch);
    RUN_TEST(test_decodingFullres16chLow);

    UNITY_END();

    return 0;
}
