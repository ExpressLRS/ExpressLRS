/**
 * This file is part of ExpressLRS
 * See https://github.com/AlessandroAU/ExpressLRS
 *
 * Unit tests for over the air packet encoding, decoding and associated utilities
 *
 * Entry point is setup_switches()
 */

#include <Arduino.h>
#include <unity.h>

#include "LoRaRadioLib.h"
#include "CRSF.h"       // has to come after LoraRadioLib.h for R9
#include "targets.h"
#include "common.h"
#include <OTA.h>

// Tests have only been run on TTGO V1, so we have to guard
// all the testcode to prevent breakages until it's know to do
// something sensible.

#ifdef TARGET_TTGO_LORA_V1_AS_TX

CRSF crsf;  // need an instance to provide the fields used by the code under test

SX127xDriver Radio; // needed for Radio.TXdataBuffer

/* Check that the round robin works
 * First call should return 0 for seq switches or 1 for hybrid
 * Successive calls should increment the next index until wrap
 * around from 7 to either 0 or 1 depending on mode.
 */
void test_round_robin(void) {

    uint8_t expectedIndex=crsf.nextSwitchIndex;

    for(uint8_t i=0; i<10; i++) {
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
void test_priority(void) {

    uint8_t nsi;

    crsf.nextSwitchIndex=0; // this would be the next switch if nothing changed

    // set all switches and sent values to be equal
    for(uint8_t i=0; i<N_SWITCHES; i++) {
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
    uint8_t UID[6] = {MY_UID};
    uint8_t DeviceAddr = UID[5] & 0b111111;
    uint8_t expected;

    // Define the input data
    // 4 channels of analog data
    crsf.ChannelDataIn[0] = 0x0123;
    crsf.ChannelDataIn[1] = 0x4567;
    crsf.ChannelDataIn[2] = 0x89AB;
    crsf.ChannelDataIn[3] = 0xCDEF;

    // 8 switches
    for(int i=0; i<N_SWITCHES; i++) {
        crsf.currentSwitches[i] =  i % 3;
        crsf.sentSwitches[i] = i % 3; // make all the sent values match
    }

    // set the nextSwitchIndex so we know which switch to expect in the packet
    crsf.nextSwitchIndex=3;

    // encode it
    GenerateChannelDataHybridSwitch8(&Radio, &crsf, DeviceAddr);

    // check it looks right
    // 1st byte is header & packet type
    uint8_t header = (DeviceAddr << 2) + RC_DATA_PACKET;
    TEST_ASSERT_EQUAL(header, Radio.TXdataBuffer[0]);

    // bytes 1 through 5 are 10 bit packed analog channels
    for(int i=0; i<4; i++) {
        expected = crsf.ChannelDataIn[i] >> 3; // most significant 8 bits
        TEST_ASSERT_EQUAL(expected, Radio.TXdataBuffer[i+1]);
    }

    // byte 5 is bits 1 and 2 of each analog channel
    expected = 0;
    for(int i=0; i<4; i++) {
        expected = (expected <<2) | ((crsf.ChannelDataIn[i] >> 1) & 0b11);
    }
    TEST_ASSERT_EQUAL(expected, Radio.TXdataBuffer[5]);

    // byte 6 is the switch encoding
    // expect switch 0 in bits 5 and 6, index in 2-4 and value in 0,1
    // top bit is undefined
    TEST_ASSERT_EQUAL(crsf.currentSwitches[0], (Radio.TXdataBuffer[6] & 0b01100000)>>5);
    TEST_ASSERT_EQUAL(3, (Radio.TXdataBuffer[6] & 0b11100)>>2);
    TEST_ASSERT_EQUAL(crsf.currentSwitches[3], Radio.TXdataBuffer[6] & 0b11);
}

/* Check the decoding of a packet after rx
*/
void test_decodingHybrid8()
{
    uint8_t UID[6] = {MY_UID};
    uint8_t DeviceAddr = UID[5] & 0b111111;
    // uint8_t expected;

    // Define the input data
    // 4 channels of analog data
    crsf.ChannelDataIn[0] = 0x0123;
    crsf.ChannelDataIn[1] = 0x4567;
    crsf.ChannelDataIn[2] = 0x89AB;
    crsf.ChannelDataIn[3] = 0xCDEF;

    // 8 switches
    for(int i=0; i<N_SWITCHES; i++) {
        crsf.currentSwitches[i] =  i % 3;
        crsf.sentSwitches[i] = i % 3; // make all the sent values match
    }

    // set the nextSwitchIndex so we know which switch to expect in the packet
    crsf.nextSwitchIndex=3;

    // use the encoding method to pack it into Radio.TXdataBuffer
    GenerateChannelDataHybridSwitch8(&Radio, &crsf, DeviceAddr);

    // copy into the expected buffer for the decoder
    memcpy((void*)Radio.RXdataBuffer, (const void*)Radio.TXdataBuffer, sizeof(Radio.RXdataBuffer));

    // run the decoder, results in crsf->PackedRCdataOut
    UnpackChannelDataHybridSwitches8(&Radio, &crsf);

    // compare the unpacked results with the input data
    TEST_ASSERT_EQUAL(crsf.ChannelDataIn[0] & 0b11111111110, crsf.PackedRCdataOut.ch0); // analog channels are truncated to 10 bits
    TEST_ASSERT_EQUAL(crsf.ChannelDataIn[1] & 0b11111111110, crsf.PackedRCdataOut.ch1); // analog channels are truncated to 10 bits
    TEST_ASSERT_EQUAL(crsf.ChannelDataIn[2] & 0b11111111110, crsf.PackedRCdataOut.ch2); // analog channels are truncated to 10 bits
    TEST_ASSERT_EQUAL(crsf.ChannelDataIn[3] & 0b11111111110, crsf.PackedRCdataOut.ch3); // analog channels are truncated to 10 bits

    TEST_ASSERT_EQUAL(SWITCH2b_to_CRSF(crsf.currentSwitches[0] & 0b11), crsf.PackedRCdataOut.ch4); // Switch 0 is sent on every packet
    TEST_ASSERT_EQUAL(SWITCH2b_to_CRSF(crsf.currentSwitches[3] & 0b11), crsf.PackedRCdataOut.ch7); // We forced switch 3 to be sent as the sequential field
}


// ------------------------------------------------------
// Test the sequential switches encoding/decoding

// encoding
void test_encodingSEQ()
{
    uint8_t UID[6] = {MY_UID};
    uint8_t DeviceAddr = UID[5] & 0b111111;
    uint8_t expected;

    // Define the input data
    // 4 channels of analog data
    crsf.ChannelDataIn[0] = 0x0123;
    crsf.ChannelDataIn[1] = 0x4567;
    crsf.ChannelDataIn[2] = 0x89AB;
    crsf.ChannelDataIn[3] = 0xCDEF;

    // 8 switches
    for(int i=0; i<N_SWITCHES; i++) {
        crsf.currentSwitches[i] =  i % 3;
        crsf.sentSwitches[i] = i % 3; // make all the sent values match
    }

    // set the nextSwitchIndex so we know which switch to expect in the packet
    crsf.nextSwitchIndex=3;

    // encode it
    GenerateChannelDataSeqSwitch(&Radio, &crsf, DeviceAddr);

    // check it looks right
    // 1st byte is header & packet type
    uint8_t header = (DeviceAddr << 2) + RC_DATA_PACKET;
    TEST_ASSERT_EQUAL(header, Radio.TXdataBuffer[0]);

    // bytes 1 through 4 are the high bits of the analog channels
    for(int i=0; i<4; i++) {
        expected = crsf.ChannelDataIn[i] >> 3; // most significant 8 bits
        TEST_ASSERT_EQUAL(expected, Radio.TXdataBuffer[i+1]);
    }

    // byte 5 contains some bits from the first three channels
    expected = (crsf.ChannelDataIn[0] & 0b111) << 5 | (crsf.ChannelDataIn[1] & 0b111) << 2 | (crsf.ChannelDataIn[2] & 0b110) >> 1;
    TEST_ASSERT_EQUAL(expected, Radio.TXdataBuffer[5]);

    // bit 7 of byte 6 contains the lsb of ChannelDataIn[2], 
    expected = crsf.ChannelDataIn[2] & 0b1;
    TEST_ASSERT_EQUAL(expected, (Radio.TXdataBuffer[6] & 0b10000000) >> 7);

    // bits 6,5 contain bits 1,2 of ChannelDataIn[3]
    expected = (crsf.ChannelDataIn[3] & 0b110) >> 1;
    TEST_ASSERT_EQUAL(expected, (Radio.TXdataBuffer[6] & 0b1100000) >> 5);

    // the sequential switch bits:
    // expect index in 2-4 and value in 0,1
    TEST_ASSERT_EQUAL(3, (Radio.TXdataBuffer[6] & 0b11100)>>2);
    TEST_ASSERT_EQUAL(crsf.currentSwitches[3], Radio.TXdataBuffer[6] & 0b11);
}

// decoding
/* Check the decoding of a packet after rx
*/
void test_decodingSEQ()
{
    uint8_t UID[6] = {MY_UID};
    uint8_t DeviceAddr = UID[5] & 0b111111;
    // uint8_t expected;

    // Define the input data
    // 4 channels of analog data
    crsf.ChannelDataIn[0] = 0x0123;
    crsf.ChannelDataIn[1] = 0x4567;
    crsf.ChannelDataIn[2] = 0x89AB;
    crsf.ChannelDataIn[3] = 0xCDEF;

    // 8 switches
    for(int i=0; i<N_SWITCHES; i++) {
        crsf.currentSwitches[i] =  (i+1) % 3;
        crsf.sentSwitches[i] = (i+1) % 3; // make all the sent values match
    }

    // set the nextSwitchIndex so we know which switch to expect in the packet
    crsf.nextSwitchIndex=3;

    // use the encoding method to pack it into Radio.TXdataBuffer
    GenerateChannelDataSeqSwitch(&Radio, &crsf, DeviceAddr);

    // copy into the required buffer for the decoder
    memcpy((void*)Radio.RXdataBuffer, (const void*)Radio.TXdataBuffer, sizeof(Radio.RXdataBuffer));
    for(int i=0;i<8; i++) Serial.printf("%d %d\n", Radio.RXdataBuffer[i], Radio.TXdataBuffer[i]);

    // clear the output buffer to avoid cross-talk between tests
    memset((void *) &crsf.PackedRCdataOut, 0, sizeof(crsf.PackedRCdataOut));

    // run the decoder, results in crsf->PackedRCdataOut
    UnpackChannelDataSeqSwitches(&Radio, &crsf);

    // compare the unpacked results with the input data
    TEST_ASSERT_EQUAL(crsf.ChannelDataIn[0] & 0b11111111111, crsf.PackedRCdataOut.ch0); // contains 11 bit data
    TEST_ASSERT_EQUAL(crsf.ChannelDataIn[1] & 0b11111111111, crsf.PackedRCdataOut.ch1); // contains 11 bit data
    TEST_ASSERT_EQUAL(crsf.ChannelDataIn[2] & 0b11111111111, crsf.PackedRCdataOut.ch2); // contains 11 bit data
    TEST_ASSERT_EQUAL(crsf.ChannelDataIn[3] & 0b11111111110, crsf.PackedRCdataOut.ch3); // last analog channel is truncated to 10 bits

    TEST_ASSERT_EQUAL(SWITCH2b_to_CRSF(crsf.currentSwitches[3] & 0b11), crsf.PackedRCdataOut.ch7); // We forced switch 3 to be sent as the sequential field
}


#endif // TARGET_TTGO_LORA_V1_AS_TX

void setup_switches()
{
    // tests have only been run on ttgo v1
    #ifdef TARGET_TTGO_LORA_V1_AS_TX

    RUN_TEST(test_round_robin);
    RUN_TEST(test_priority);

    // #ifdef HYBRID_SWITCHES_8
    RUN_TEST(test_encodingHybrid8);
    RUN_TEST(test_decodingHybrid8);
    // #endif // HYBRID_SWITCHES_8

    RUN_TEST(test_encodingSEQ);
    RUN_TEST(test_decodingSEQ);

    #endif // TARGET_TTGO_LORA_V1_AS_TX
}
