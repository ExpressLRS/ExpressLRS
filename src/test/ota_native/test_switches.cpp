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
#include "channels.h"
#include "CRSF.h"
#include <OTA.h>

HardwareSerial CRSF_Port = HardwareSerial();

/* Check that the round robin works
 * First call should return 0 for seq switches or 1 for hybrid
 * Successive calls should increment the next index until wrap
 * around from 7 to either 0 or 1 depending on mode.
 */
void test_round_robin(void)
{
    uint8_t expectedIndex = ota.NextSwitchIndex;

    for(uint8_t i = 0; i < 10; i++) {
        uint8_t nsi = ota.getNextSwitchIndex();
        TEST_ASSERT_EQUAL(expectedIndex, nsi);
        expectedIndex++;
        if (expectedIndex == 8) {
            #ifdef HYBRID_SWITCHES_8
            expectedIndex = 1;
            #else
            expectedIndex = 0;
            #endif
        }
    }
}

/* Check that a changed switch gets priority
*/
void test_priority(void)
{
    uint8_t nsi;

    ota.NextSwitchIndex = 0; // this would be the next switch if nothing changed

    // set all switches and sent values to be equal
    for(uint8_t i = 0; i < N_SWITCHES; i++) {
        ota.setSentSwitch(i, 0);
        ota.setCurrentSwitch(i, 0);
    }

    // set two switches' current value to be different
    ota.setCurrentSwitch(4, 1);
    ota.setCurrentSwitch(6, 1);

    // we expect to get the lowest changed switch
    nsi = ota.getNextSwitchIndex();
    TEST_ASSERT_EQUAL(4, nsi);

    // The sending code would then change the sent value to match:
    ota.setSentSwitch(4, 1);

    // so now we expect to get 6 (the other changed switch we set above)
    nsi = ota.getNextSwitchIndex();
    TEST_ASSERT_EQUAL(6, nsi);

    // The sending code would then change the sent value to match:
    ota.setSentSwitch(6, 1);

    // Now all sent values should match the current values, and we expect
    // to get the last returned value +1
    nsi = ota.getNextSwitchIndex();
    TEST_ASSERT_EQUAL(7, nsi);
}

// ------------------------------------------------
// Test the hybrid8 encoding/decoding

/* Check the hybrid 8 encoding of a packet for OTA tx
*/
void test_encodingHybrid8(bool highResChannel)
{
    uint8_t UID[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE};
    uint8_t expected;
    uint8_t TXdataBuffer[8];

    // Define the input data
    // 4 channels of analog data
    channels.ChannelData[0] = 0x0123;
    channels.ChannelData[1] = 0x4567;
    channels.ChannelData[2] = 0x89AB;
    channels.ChannelData[3] = 0xCDEF;

    // 8 switches
    for(int i = 0; i < N_SWITCHES; i++) {
        ota.setCurrentSwitch(i, i % 3);
        ota.setSentSwitch(i, i % 3); // make all the sent values match
    }

    // set the nextSwitchIndex so we know which switch to expect in the packet
    if (highResChannel)
        ota.NextSwitchIndex = 7;
    else
        ota.NextSwitchIndex = 3;

    // encode it
    ota.GenerateChannelDataHybridSwitch8(TXdataBuffer, &channels, false);

    // check it looks right
    // 1st byte is CRC & packet type
    uint8_t header = RC_DATA_PACKET;
    TEST_ASSERT_EQUAL(header, TXdataBuffer[0]);

    // bytes 1 through 5 are 10 bit packed analog channels
    for(int i = 0; i < 4; i++) {
        expected = channels.ChannelData[i] >> 3; // most significant 8 bits
        TEST_ASSERT_EQUAL(expected, TXdataBuffer[i + 1]);
    }

    // byte 5 is bits 1 and 2 of each analog channel
    expected = 0;
    for(int i = 0; i < 4; i++) {
        expected = (expected <<2) | ((channels.ChannelData[i] >> 1) & 0b11);
    }
    TEST_ASSERT_EQUAL(expected, TXdataBuffer[5]);

    // byte 6 is the switch encoding
    TEST_ASSERT_EQUAL(ota.CurrentSwitches[0], (TXdataBuffer[6] & 0b0100000)>>6);
    // top bit is undefined
    // expect switch 0 in bit 6
    // index-1 in 3-5
    // value in 0,1,2[,3]
    if (highResChannel)
    {
        TEST_ASSERT_EQUAL(7, ((TXdataBuffer[6] & 0b110000)>>3) + 1);
        TEST_ASSERT_EQUAL(ota.CurrentSwitches[7], TXdataBuffer[6] & 0b1111);
    }
    else
    {
        TEST_ASSERT_EQUAL(3, ((TXdataBuffer[6] & 0b111000)>>3) + 1);
        TEST_ASSERT_EQUAL(ota.CurrentSwitches[3], TXdataBuffer[6] & 0b0111);
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
    uint8_t UID[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE};
    uint8_t TXdataBuffer[8];
    // uint8_t expected;

    // Define the input data
    // 4 channels of analog data
    channels.ChannelData[0] = 0x0123;
    channels.ChannelData[1] = 0x4567;
    channels.ChannelData[2] = 0x89AB;
    channels.ChannelData[3] = 0xCDEF;

    // 8 switches
    for(int i = 0; i < N_SWITCHES; i++) {
        ota.setCurrentSwitch(i, i % 3);
        ota.setSentSwitch(i, i % 3); // make all the sent values match
    }
    ota.setCurrentSwitch(forceSwitch, switchval);
    ota.setSentSwitch(forceSwitch, switchval);

    // set the nextSwitchIndex so we know which switch to expect in the packet
    // nextSwitchIndex=0 is invalid, since the previous getNextSwitchIndex()
    // would have skipped it
    if (forceSwitch == 0)
        ota.NextSwitchIndex = 1;
    else
        ota.NextSwitchIndex = forceSwitch;

    // use the encoding method to pack it into TXdataBuffer
    ota.GenerateChannelDataHybridSwitch8(TXdataBuffer, &channels, false);

    // run the decoder, results in crsf->PackedRCdataOut
    ota.UnpackChannelDataHybridSwitch8(TXdataBuffer, &channels);

    // compare the unpacked results with the input data
    TEST_ASSERT_EQUAL(channels.ChannelData[0] & 0b11111111110, channels.PackedRCdataOut.ch0); // analog channels are truncated to 10 bits
    TEST_ASSERT_EQUAL(channels.ChannelData[1] & 0b11111111110, channels.PackedRCdataOut.ch1); // analog channels are truncated to 10 bits
    TEST_ASSERT_EQUAL(channels.ChannelData[2] & 0b11111111110, channels.PackedRCdataOut.ch2); // analog channels are truncated to 10 bits
    TEST_ASSERT_EQUAL(channels.ChannelData[3] & 0b11111111110, channels.PackedRCdataOut.ch3); // analog channels are truncated to 10 bits

    TEST_ASSERT_EQUAL(BIT_to_CRSF(ota.CurrentSwitches[0]), channels.PackedRCdataOut.ch4); // Switch 0 is sent on every packet
    if (forceSwitch == 7)
        TEST_ASSERT_EQUAL(N_to_CRSF(ota.CurrentSwitches[forceSwitch], 15), channels.PackedRCdataOut.ch11); // We forced switch 1 to be sent as the sequential field
    else if (forceSwitch != 0)
    {
        uint16_t ch;
        switch (forceSwitch)
        {
        case 1: ch = channels.PackedRCdataOut.ch5; break;
        case 2: ch = channels.PackedRCdataOut.ch6; break;
        case 3: ch = channels.PackedRCdataOut.ch7; break;
        case 4: ch = channels.PackedRCdataOut.ch8; break;
        case 5: ch = channels.PackedRCdataOut.ch9; break;
        case 6: ch = channels.PackedRCdataOut.ch10; break;
        default:
            TEST_FAIL_MESSAGE("forceSwitch not handled");
        }
        TEST_ASSERT_EQUAL(SWITCH3b_to_CRSF(ota.CurrentSwitches[forceSwitch]), channels.PackedRCdataOut.ch7); // We forced switch 3 to be sent as the sequential field
    }
}

void test_decodingHybrid8_all()
{
    // Switch 0 is 2 pos
    test_decodingHybrid8(0, 0);
    test_decodingHybrid8(0, 1);
    // Switch X in 6-pos mode (includes 3-pos low/high)
    for (uint8_t val=0; val<6; ++val)
        test_decodingHybrid8(3, val);
    // Switch X in 3-pos mode center
    test_decodingHybrid8(3, 7);
    // Switch 7 is 16 pos
    for (uint8_t val=0; val<16; ++val)
        test_decodingHybrid8(7, val);
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
    channels.ChannelData[0] = 0x0123;
    channels.ChannelData[1] = 0x4567;
    channels.ChannelData[2] = 0x89AB;
    channels.ChannelData[3] = 0xCDEF;

    // 8 switches
    for(int i = 4; i < 12; i++) {
        channels.ChannelData[i] =  i % 2 * 1800;
    }

    // encode it
    ota.GenerateChannelData10bit(TXdataBuffer, &channels, false);

    // check it looks right
    // 1st byte is CRC & packet type
    uint8_t header = RC_DATA_PACKET;
    TEST_ASSERT_EQUAL(header, TXdataBuffer[0]);

    // bytes 1 through 5 are 10 bit packed analog channels
    for(int i = 0; i < 4; i++) {
        expected = channels.ChannelData[i] >> 3; // most significant 8 bits
        TEST_ASSERT_EQUAL(expected, TXdataBuffer[i + 1]);
    }

    // byte 5 is bits 1 and 2 of each analog channel
    expected = 0;
    for(int i = 0; i < 4; i++) {
        expected = (expected <<2) | ((channels.ChannelData[i] >> 1) & 0b11);
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
    channels.ChannelData[0] = 0x0123;
    channels.ChannelData[1] = 0x4567;
    channels.ChannelData[2] = 0x89AB;
    channels.ChannelData[3] = 0xCDEF;

    // 8 switches
    for(int i = 4; i < 12; i++) {
        channels.ChannelData[i] =  i % 2 * 1800;
    }

    // use the encoding method to pack it into TXdataBuffer
    ota.GenerateChannelData10bit(TXdataBuffer, &channels, false);

    // run the decoder, results in crsf->PackedRCdataOut
    ota.UnpackChannelData10bit(TXdataBuffer, &channels);

    // compare the unpacked results with the input data
    TEST_ASSERT_EQUAL(channels.ChannelData[0] & 0b11111111110, channels.PackedRCdataOut.ch0); // analog channels are truncated to 10 bits
    TEST_ASSERT_EQUAL(channels.ChannelData[1] & 0b11111111110, channels.PackedRCdataOut.ch1); // analog channels are truncated to 10 bits
    TEST_ASSERT_EQUAL(channels.ChannelData[2] & 0b11111111110, channels.PackedRCdataOut.ch2); // analog channels are truncated to 10 bits
    TEST_ASSERT_EQUAL(channels.ChannelData[3] & 0b11111111110, channels.PackedRCdataOut.ch3); // analog channels are truncated to 10 bits

    TEST_ASSERT_EQUAL(BIT_to_CRSF(0), channels.PackedRCdataOut.ch4); // Switch 0
    TEST_ASSERT_EQUAL(BIT_to_CRSF(1), channels.PackedRCdataOut.ch5); // Switch 1
    TEST_ASSERT_EQUAL(BIT_to_CRSF(0), channels.PackedRCdataOut.ch6); // Switch 2
    TEST_ASSERT_EQUAL(BIT_to_CRSF(1), channels.PackedRCdataOut.ch7); // Switch 3
    TEST_ASSERT_EQUAL(BIT_to_CRSF(0), channels.PackedRCdataOut.ch8); // Switch 4
    TEST_ASSERT_EQUAL(BIT_to_CRSF(1), channels.PackedRCdataOut.ch9); // Switch 5
    TEST_ASSERT_EQUAL(BIT_to_CRSF(0), channels.PackedRCdataOut.ch10); // Switch 6
    TEST_ASSERT_EQUAL(BIT_to_CRSF(1), channels.PackedRCdataOut.ch11); // Switch 7
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_round_robin);
    RUN_TEST(test_priority);

    RUN_TEST(test_encodingHybrid8_3);
    RUN_TEST(test_encodingHybrid8_7);
    RUN_TEST(test_decodingHybrid8_all);

    RUN_TEST(test_encoding10bit);
    RUN_TEST(test_decoding10bit);

    UNITY_END();

    return 0;
}
