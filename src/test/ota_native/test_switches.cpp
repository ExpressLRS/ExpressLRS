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
#include "CRSF.h"       // has to come after SX127xDriver.h for R9
#include <OTA.h>

CRSF crsf(NULL);  // need an instance to provide the fields used by the code under test
HardwareSerial CRSF::Port = HardwareSerial();

/* Check that the round robin works
 * First call should return 0 for seq switches or 1 for hybrid
 * Successive calls should increment the next index until wrap
 * around from 7 to either 0 or 1 depending on mode.
 */
void test_round_robin(void)
{
    uint8_t expectedIndex = crsf.nextSwitchIndex;

    for(uint8_t i = 0; i < 10; i++) {
        uint8_t nsi = crsf.getNextSwitchIndex();
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

    crsf.nextSwitchIndex = 0; // this would be the next switch if nothing changed

    // set all switches and sent values to be equal
    for(uint8_t i = 0; i < N_SWITCHES; i++) {
        crsf.sentSwitches[i] = 0;
        crsf.currentSwitches[i] = 0;
    }

    // set two switches' current value to be different
    crsf.currentSwitches[4] = 1;
    crsf.currentSwitches[6] = 1;

    // we expect to get the lowest changed switch
    nsi = crsf.getNextSwitchIndex();
    TEST_ASSERT_EQUAL(4, nsi);

    // The sending code would then change the sent value to match:
    crsf.sentSwitches[4] = 1;

    // so now we expect to get 6 (the other changed switch we set above)
    nsi = crsf.getNextSwitchIndex();
    TEST_ASSERT_EQUAL(6, nsi);

    // The sending code would then change the sent value to match:
    crsf.sentSwitches[6] = 1;

    // Now all sent values should match the current values, and we expect
    // to get the last returned value +1
    nsi = crsf.getNextSwitchIndex();
    TEST_ASSERT_EQUAL(7, nsi);
}

// ------------------------------------------------
// Test the hybrid8 encoding/decoding

/* Check the hybrid 8 encoding of a packet for OTA tx
*/
void test_encodingHybrid8()
{
    uint8_t UID[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE};
    uint8_t DeviceAddr = UID[5] & 0b111111;
    uint8_t expected;
    uint8_t TXdataBuffer[8];

    // Define the input data
    // 4 channels of analog data
    crsf.ChannelDataIn[0] = 0x0123;
    crsf.ChannelDataIn[1] = 0x4567;
    crsf.ChannelDataIn[2] = 0x89AB;
    crsf.ChannelDataIn[3] = 0xCDEF;

    // 8 switches
    for(int i = 0; i < N_SWITCHES; i++) {
        crsf.currentSwitches[i] =  i % 3;
        crsf.sentSwitches[i] = i % 3; // make all the sent values match
    }

    // set the nextSwitchIndex so we know which switch to expect in the packet
    crsf.nextSwitchIndex = 3;

    // encode it
    GenerateChannelDataHybridSwitch8(TXdataBuffer, &crsf, DeviceAddr, false);

    // check it looks right
    // 1st byte is header & packet type
    uint8_t header = (DeviceAddr << 2) + RC_DATA_PACKET;
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
    // expect switch 0 in bits 5 and 6, index in 2-4 and value in 0,1
    // top bit is undefined
    TEST_ASSERT_EQUAL(crsf.currentSwitches[0], (TXdataBuffer[6] & 0b01100000)>>5);
    TEST_ASSERT_EQUAL(3, (TXdataBuffer[6] & 0b11100)>>2);
    TEST_ASSERT_EQUAL(crsf.currentSwitches[3], TXdataBuffer[6] & 0b11);
}

/* Check the decoding of a packet after rx
*/
void test_decodingHybrid8()
{
    uint8_t UID[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE};
    uint8_t DeviceAddr = UID[5] & 0b111111;
    uint8_t TXdataBuffer[8];
    // uint8_t expected;

    // Define the input data
    // 4 channels of analog data
    crsf.ChannelDataIn[0] = 0x0123;
    crsf.ChannelDataIn[1] = 0x4567;
    crsf.ChannelDataIn[2] = 0x89AB;
    crsf.ChannelDataIn[3] = 0xCDEF;

    // 8 switches
    for(int i = 0; i < N_SWITCHES; i++) {
        crsf.currentSwitches[i] =  i % 3;
        crsf.sentSwitches[i] = i % 3; // make all the sent values match
    }

    // set the nextSwitchIndex so we know which switch to expect in the packet
    crsf.nextSwitchIndex = 3;

    // use the encoding method to pack it into TXdataBuffer
    GenerateChannelDataHybridSwitch8(TXdataBuffer, &crsf, DeviceAddr, false);

    // run the decoder, results in crsf->PackedRCdataOut
    UnpackChannelDataHybridSwitch8(TXdataBuffer, &crsf);

    // compare the unpacked results with the input data
    TEST_ASSERT_EQUAL(crsf.ChannelDataIn[0] & 0b11111111110, crsf.PackedRCdataOut.ch0); // analog channels are truncated to 10 bits
    TEST_ASSERT_EQUAL(crsf.ChannelDataIn[1] & 0b11111111110, crsf.PackedRCdataOut.ch1); // analog channels are truncated to 10 bits
    TEST_ASSERT_EQUAL(crsf.ChannelDataIn[2] & 0b11111111110, crsf.PackedRCdataOut.ch2); // analog channels are truncated to 10 bits
    TEST_ASSERT_EQUAL(crsf.ChannelDataIn[3] & 0b11111111110, crsf.PackedRCdataOut.ch3); // analog channels are truncated to 10 bits

    TEST_ASSERT_EQUAL(SWITCH2b_to_CRSF(crsf.currentSwitches[0] & 0b11), crsf.PackedRCdataOut.ch4); // Switch 0 is sent on every packet
    TEST_ASSERT_EQUAL(SWITCH2b_to_CRSF(crsf.currentSwitches[3] & 0b11), crsf.PackedRCdataOut.ch7); // We forced switch 3 to be sent as the sequential field
}


int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_round_robin);
    RUN_TEST(test_priority);

    RUN_TEST(test_encodingHybrid8);
    RUN_TEST(test_decodingHybrid8);

    UNITY_END();

    return 0;
}
