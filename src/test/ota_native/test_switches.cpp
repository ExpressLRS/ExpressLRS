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
// #include "common.h"
#include "CRSF.h"
#include <OTA.h>

CRSF crsf(NULL);  // need an instance to provide the fields used by the code under test
HardwareSerial CRSF::Port = HardwareSerial();

/* Check that the round robin works
 * First call should return 0 for seq switches or 1 for hybrid
 * Successive calls should increment the next index until wrap
 * around from 7 to either 0 or 1 depending on mode.
 */
// void test_round_robin(void)
// {
//     uint8_t expectedIndex = crsf.nextSwitchIndex;

//     for(uint8_t i = 0; i < 10; i++) {
//         uint8_t nsi = crsf.getNextSwitchIndex();
//         TEST_ASSERT_EQUAL(expectedIndex, nsi);
//         expectedIndex++;
//         if (expectedIndex == 8) {
//             #ifdef HYBRID_SWITCHES_8
//             expectedIndex = 1;
//             #else
//             expectedIndex = 0;
//             #endif
//         }
//     }
// }

/* Check that a changed switch gets priority
*/
// void test_priority(void)
// {
//     uint8_t nsi;

//     crsf.nextSwitchIndex = 0; // this would be the next switch if nothing changed

//     // set all switches and sent values to be equal
//     for(uint8_t i = 0; i < N_SWITCHES; i++) {
//         crsf.sentSwitches[i] = 0;
//         crsf.currentSwitches[i] = 0;
//     }

//     // set two switches' current value to be different
//     crsf.currentSwitches[4] = 1;
//     crsf.currentSwitches[6] = 1;

//     // we expect to get the lowest changed switch
//     nsi = crsf.getNextSwitchIndex();
//     TEST_ASSERT_EQUAL(4, nsi);

//     // The sending code would then change the sent value to match:
//     crsf.sentSwitches[4] = 1;

//     // so now we expect to get 6 (the other changed switch we set above)
//     nsi = crsf.getNextSwitchIndex();
//     TEST_ASSERT_EQUAL(6, nsi);

//     // The sending code would then change the sent value to match:
//     crsf.sentSwitches[6] = 1;

//     // Now all sent values should match the current values, and we expect
//     // to get the last returned value +1
//     nsi = crsf.getNextSwitchIndex();
//     TEST_ASSERT_EQUAL(7, nsi);
// }

// ------------------------------------------------
// Test the hybrid8 encoding/decoding

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

/* Check the hybrid 8 encoding of a packet for OTA tx
*/
void test_encodingHybrid8(bool highRes, uint8_t nonce)
{
    uint8_t UID[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE};
    uint8_t expected;
    uint8_t TXdataBuffer[8];

    // Define the input data
    // 4 channels of analog data
    crsf.ChannelDataIn[0] = 0x0123;
    crsf.ChannelDataIn[1] = 0x4567;
    crsf.ChannelDataIn[2] = 0x89AB;
    crsf.ChannelDataIn[3] = 0xCDEF;

    // 8 switches
    constexpr int N_SWITCHES = 8;
    for(int i = 0; i < N_SWITCHES; i++) {
        constexpr int CHANNELS[] =
            { CRSF_CHANNEL_VALUE_1000, CRSF_CHANNEL_VALUE_MID, CRSF_CHANNEL_VALUE_2000 };
        crsf.ChannelDataIn[4+i] = CHANNELS[i % 3];
    }

    // Uplink data
    crsf.LinkStatistics.uplink_TX_Power = 3; // 100mW

    // encode it
    uint8_t tlmDenom = (highRes) ? 64 : 4;
    GenerateChannelDataHybridSwitch8(TXdataBuffer, &crsf, nonce, tlmDenom, nonce % 2);

    // check it looks right
    // 1st byte is CRC & packet type
    uint8_t header = RC_DATA_PACKET;
    TEST_ASSERT_EQUAL(header, TXdataBuffer[0]);

    // bytes 1 through 5 are 10 bit packed analog channels
    for(int i = 0; i < 4; i++) {
        expected = crsf.ChannelDataIn[i] >> 3; // most significant 8 bits
        TEST_ASSERT_EQUAL(expected, TXdataBuffer[i + 1]);
    }

    // byte 5 is bits 1 and 2 of each analog channel
    expected = 0;
    for(int i = 0; i < 4; i++) {
        expected = (expected <<2) | ((crsf.ChannelDataIn[i] >> 1) & 0b11);
    }
    TEST_ASSERT_EQUAL(expected, TXdataBuffer[5]);

    // byte 6 is the switches encoded
    uint8_t switches = TXdataBuffer[6];
    uint8_t switchIdx = nonce % 8;

    // High bit should be AUX1
    TEST_ASSERT_EQUAL(CRSF_to_BIT(crsf.ChannelDataIn[4]), switches >> 7);
    // If low res or slot 7, the bit 6 should be the telemetryack bit
    if (!highRes || switchIdx == 7)
        TEST_ASSERT_EQUAL(nonce % 2, (switches >> 6) & 1);

    // If slot 7, the uplink_TX_Power should be in the low 6 bits
    if (switchIdx == 7)
        TEST_ASSERT_EQUAL(crsf.LinkStatistics.uplink_TX_Power, switches & 0b111111);
    else
    {
        uint16_t ch = crsf.ChannelDataIn[5+switchIdx];
        if (highRes)
            TEST_ASSERT_EQUAL(CRSF_to_N(ch, 128), switches & 0b1111111); // 7-bit
        else
            TEST_ASSERT_EQUAL(CRSF_to_N(ch, 64), switches & 0b111111); // 6-bit
    }
}

void test_encodingHybrid8_high()
{
    constexpr int N_SWITCHES = 8;
    for (int i=0; i<N_SWITCHES; ++i)
        test_encodingHybrid8(true, i);
}

void test_encodingHybrid8_low()
{
    constexpr int N_SWITCHES = 8;
    for (int i=0; i<N_SWITCHES; ++i)
        test_encodingHybrid8(false, i);
}

/* Check the decoding of a packet after rx
*/
void test_decodingHybrid8(bool highRes, uint8_t nonce, uint8_t forceSwitch, uint16_t forceVal)
{
    uint8_t UID[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE};
    uint8_t TXdataBuffer[8];
    // uint8_t expected;

    // Define the input data
    // 4 channels of analog data
    crsf.ChannelDataIn[0] = 0x0123;
    crsf.ChannelDataIn[1] = 0x4567;
    crsf.ChannelDataIn[2] = 0x89AB;
    crsf.ChannelDataIn[3] = 0xCDEF;

    // 8 switches
    constexpr int N_SWITCHES = 8;
    for(int i = 0; i < N_SWITCHES; i++) {
        constexpr int CHANNELS[] =
            { CRSF_CHANNEL_VALUE_1000, CRSF_CHANNEL_VALUE_MID, CRSF_CHANNEL_VALUE_2000 };
        if (i == forceSwitch)
            crsf.ChannelDataIn[4+i] = forceVal;
        else
            crsf.ChannelDataIn[4+i] = CHANNELS[i % 3];
    }

    // Uplink data
    crsf.LinkStatistics.uplink_TX_Power = 3; // 100mW

    // encode it
    uint8_t tlmDenom = (highRes) ? 64 : 4;
    GenerateChannelDataHybridSwitch8(TXdataBuffer, &crsf, nonce, tlmDenom, nonce % 2);

    // Clear the LinkStatistics to receive it from the encoding
    crsf.LinkStatistics.uplink_TX_Power = 0;

    // run the decoder, results in crsf->PackedRCdataOut
    bool telemResult = UnpackChannelDataHybridSwitch8(TXdataBuffer, &crsf, nonce, tlmDenom);

    // compare the unpacked results with the input data
    TEST_ASSERT_EQUAL(crsf.ChannelDataIn[0] & 0b11111111110, crsf.PackedRCdataOut.ch0); // analog channels are truncated to 10 bits
    TEST_ASSERT_EQUAL(crsf.ChannelDataIn[1] & 0b11111111110, crsf.PackedRCdataOut.ch1); // analog channels are truncated to 10 bits
    TEST_ASSERT_EQUAL(crsf.ChannelDataIn[2] & 0b11111111110, crsf.PackedRCdataOut.ch2); // analog channels are truncated to 10 bits
    TEST_ASSERT_EQUAL(crsf.ChannelDataIn[3] & 0b11111111110, crsf.PackedRCdataOut.ch3); // analog channels are truncated to 10 bits

    // Switch 0 is sent on every packet
    TEST_ASSERT_EQUAL(crsf.ChannelDataIn[4], crsf.PackedRCdataOut.ch4);

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
        uint16_t ch;
        switch (switchIdx)
        {
        case 0: ch = crsf.PackedRCdataOut.ch5; break;
        case 1: ch = crsf.PackedRCdataOut.ch6; break;
        case 2: ch = crsf.PackedRCdataOut.ch7; break;
        case 3: ch = crsf.PackedRCdataOut.ch8; break;
        case 4: ch = crsf.PackedRCdataOut.ch9; break;
        case 5: ch = crsf.PackedRCdataOut.ch10; break;
        case 6: ch = crsf.PackedRCdataOut.ch11; break;
        default:
            TEST_FAIL_MESSAGE("switchIdx not handled");
        }
        if (highRes)
            TEST_ASSERT_EQUAL(N_to_CRSF(CRSF_to_N(crsf.ChannelDataIn[5+switchIdx], 128), 127), ch);
        else
            TEST_ASSERT_EQUAL(N_to_CRSF(CRSF_to_N(crsf.ChannelDataIn[5+switchIdx], 64), 63), ch);
    }
}

void test_decodingHybrid8_AUX1()
{
    // Switch 0 is 2 pos, also tests the uplink_TX_Power
    test_decodingHybrid8(true, 7, 0, CRSF_CHANNEL_VALUE_1000);
    test_decodingHybrid8(true, 7, 0, CRSF_CHANNEL_VALUE_2000);
}

void test_decodingHybrid8_AUXX_high()
{
    constexpr int N_SWITCHES = 8;
    for (int i=0; i<N_SWITCHES; ++i)
        test_decodingHybrid8(true, i, 0, CRSF_CHANNEL_VALUE_1000);
}

void test_decodingHybrid8_AUXX_low()
{
    constexpr int N_SWITCHES = 8;
    for (int i=0; i<N_SWITCHES; ++i)
        test_decodingHybrid8(false, i, 0, CRSF_CHANNEL_VALUE_1000);
}

// ------------------------------------------------
// Test the 10bit encoding/decoding

/* Check the 10bit encoding of a packet for OTA tx
*/
void test_encoding10bit()
{
    uint8_t UID[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE};
    uint8_t expected;
    uint8_t TXdataBuffer[8];

    // Define the input data
    // 4 channels of analog data
    crsf.ChannelDataIn[0] = 0x0123;
    crsf.ChannelDataIn[1] = 0x4567;
    crsf.ChannelDataIn[2] = 0x89AB;
    crsf.ChannelDataIn[3] = 0xCDEF;

    // 8 switches
    for(int i = 4; i < 12; i++) {
        crsf.ChannelDataIn[i] =  i % 2 * 1800;
    }

    // encode it
    GenerateChannelData10bit(TXdataBuffer, &crsf);

    // check it looks right
    // 1st byte is CRC & packet type
    uint8_t header = RC_DATA_PACKET;
    TEST_ASSERT_EQUAL(header, TXdataBuffer[0]);

    // bytes 1 through 5 are 10 bit packed analog channels
    for(int i = 0; i < 4; i++) {
        expected = crsf.ChannelDataIn[i] >> 3; // most significant 8 bits
        TEST_ASSERT_EQUAL(expected, TXdataBuffer[i + 1]);
    }

    // byte 5 is bits 1 and 2 of each analog channel
    expected = 0;
    for(int i = 0; i < 4; i++) {
        expected = (expected <<2) | ((crsf.ChannelDataIn[i] >> 1) & 0b11);
    }
    TEST_ASSERT_EQUAL(expected, TXdataBuffer[5]);

    // byte 6 is the switch encoding
    TEST_ASSERT_EQUAL(TXdataBuffer[6], 0x55);

    for(int i = 4; i < 12; i++) {
        TEST_ASSERT_EQUAL(i%2, (TXdataBuffer[6] >> (7 - (i - 4)) ) & 1);
    }
}

/* Check the decoding of a packet after rx
*/
void test_decoding10bit()
{
    uint8_t UID[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE};
    uint8_t TXdataBuffer[8];
    // uint8_t expected;

    // Define the input data
    // 4 channels of analog data
    crsf.ChannelDataIn[0] = 0x0123;
    crsf.ChannelDataIn[1] = 0x4567;
    crsf.ChannelDataIn[2] = 0x89AB;
    crsf.ChannelDataIn[3] = 0xCDEF;

    // 8 switches
    for(int i = 4; i < 12; i++) {
        crsf.ChannelDataIn[i] =  i % 2 * 1800;
    }

    // use the encoding method to pack it into TXdataBuffer
    GenerateChannelData10bit(TXdataBuffer, &crsf);

    // run the decoder, results in crsf->PackedRCdataOut
    UnpackChannelData10bit(TXdataBuffer, &crsf);

    // compare the unpacked results with the input data
    TEST_ASSERT_EQUAL(crsf.ChannelDataIn[0] & 0b11111111110, crsf.PackedRCdataOut.ch0); // analog channels are truncated to 10 bits
    TEST_ASSERT_EQUAL(crsf.ChannelDataIn[1] & 0b11111111110, crsf.PackedRCdataOut.ch1); // analog channels are truncated to 10 bits
    TEST_ASSERT_EQUAL(crsf.ChannelDataIn[2] & 0b11111111110, crsf.PackedRCdataOut.ch2); // analog channels are truncated to 10 bits
    TEST_ASSERT_EQUAL(crsf.ChannelDataIn[3] & 0b11111111110, crsf.PackedRCdataOut.ch3); // analog channels are truncated to 10 bits

    TEST_ASSERT_EQUAL(BIT_to_CRSF(0), crsf.PackedRCdataOut.ch4); // Switch 0
    TEST_ASSERT_EQUAL(BIT_to_CRSF(1), crsf.PackedRCdataOut.ch5); // Switch 1
    TEST_ASSERT_EQUAL(BIT_to_CRSF(0), crsf.PackedRCdataOut.ch6); // Switch 2
    TEST_ASSERT_EQUAL(BIT_to_CRSF(1), crsf.PackedRCdataOut.ch7); // Switch 3
    TEST_ASSERT_EQUAL(BIT_to_CRSF(0), crsf.PackedRCdataOut.ch8); // Switch 4
    TEST_ASSERT_EQUAL(BIT_to_CRSF(1), crsf.PackedRCdataOut.ch9); // Switch 5
    TEST_ASSERT_EQUAL(BIT_to_CRSF(0), crsf.PackedRCdataOut.ch10); // Switch 6
    TEST_ASSERT_EQUAL(BIT_to_CRSF(1), crsf.PackedRCdataOut.ch11); // Switch 7
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    //RUN_TEST(test_round_robin);
    //RUN_TEST(test_priority);

    RUN_TEST(test_crsfToBit);
    RUN_TEST(test_bitToCrsf);
    RUN_TEST(test_crsfToN);
    RUN_TEST(test_nToCrsf);

    RUN_TEST(test_encodingHybrid8_high);
    RUN_TEST(test_encodingHybrid8_low);
    RUN_TEST(test_decodingHybrid8_AUX1);
    RUN_TEST(test_decodingHybrid8_AUXX_high);
    RUN_TEST(test_decodingHybrid8_AUXX_low);

    RUN_TEST(test_encoding10bit);
    RUN_TEST(test_decoding10bit);

    UNITY_END();

    return 0;
}
