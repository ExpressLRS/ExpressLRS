#include <Arduino.h>
#include <unity.h>

#include "msp.h"

// void setUp(void) {
// // set stuff up here
// }

// void tearDown(void) {
// // clean stuff up here
// }

MSP MSPProtocol;

void test_msp_receive(void)
{
    // TEST CASE:
    // GIVEN an instance of the MSP class has been instantiated
    // WHEN a valid packet is send to processReceivedByte() one byte at a time
    // THEN the complete packet will be available via getReceivedPacket()
    // AND the packet values will match the values from the sent bytes
    
    uint8_t c;
    bool packetComplete;

    c = '$';
    packetComplete = MSPProtocol.processReceivedByte(c);
    TEST_ASSERT_EQUAL(false, packetComplete);

    c = 'X';
    packetComplete = MSPProtocol.processReceivedByte(c);
    TEST_ASSERT_EQUAL(false, packetComplete);

    c = '<'; // command
    packetComplete = MSPProtocol.processReceivedByte(c);
    TEST_ASSERT_EQUAL(false, packetComplete);

    c = 0; // flags
    packetComplete = MSPProtocol.processReceivedByte(c);
    TEST_ASSERT_EQUAL(false, packetComplete);

    c = 1; // function b1
    packetComplete = MSPProtocol.processReceivedByte(c);
    TEST_ASSERT_EQUAL(false, packetComplete);

    c = 0; // function b2
    packetComplete = MSPProtocol.processReceivedByte(c);
    TEST_ASSERT_EQUAL(false, packetComplete);

    c = 1; // payload size b1
    packetComplete = MSPProtocol.processReceivedByte(c);
    TEST_ASSERT_EQUAL(false, packetComplete);

    c = 0; // payload size b2
    packetComplete = MSPProtocol.processReceivedByte(c);
    TEST_ASSERT_EQUAL(false, packetComplete);

    c = 'A'; // payload
    packetComplete = MSPProtocol.processReceivedByte(c);
    TEST_ASSERT_EQUAL(false, packetComplete);

    c = 125; // crc
    packetComplete = MSPProtocol.processReceivedByte(c);
    TEST_ASSERT_EQUAL(true, packetComplete);
  
    // Assert that values in packet match sent values
    mspPacket_t packet = MSPProtocol.getReceivedPacket();
    TEST_ASSERT_EQUAL(MSP_PACKET_COMMAND, packet.type);
    TEST_ASSERT_EQUAL(0, packet.flags);
    TEST_ASSERT_EQUAL(1, packet.function);
    TEST_ASSERT_EQUAL(1, packet.payloadSize);
    TEST_ASSERT_EQUAL('A', (char)packet.payload[0]);
}

void setup() {
    // NOTE!!! Wait for >2 secs
    // if board doesn't support software reset via Serial.DTR/RTS
    delay(2000);

    UNITY_BEGIN(); // IMPORTANT LINE!

    RUN_TEST(test_msp_receive);
}

void loop() {
    UNITY_END(); // stop unit testing
}
