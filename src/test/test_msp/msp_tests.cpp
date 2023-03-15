#include <unity.h>
#include "msp.h"
#include "common.h"
#include "mock_serial.h"

MSP MSPProtocol;
uint32_t ChannelData[CRSF_NUM_CHANNELS];      // Current state of channels, CRSF format

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
    mspPacket_t* packet = MSPProtocol.getReceivedPacket();
    TEST_ASSERT_EQUAL(MSP_PACKET_COMMAND, packet->type);
    TEST_ASSERT_EQUAL(0, packet->flags);
    TEST_ASSERT_EQUAL(1, packet->function);
    TEST_ASSERT_EQUAL(1, packet->payloadSize);
    TEST_ASSERT_EQUAL('A', (char)packet->payload[0]);
}

void test_msp_send(void)
{
    // TEST CASE:
    // GIVEN an instance of the MSP class has been instantiated
    // WHEN a valid packet is passed as an argument to sendPacket()
    // THEN the complete packet will be transmitted to the passed in Stream object
    // AND the return value will be true

    // Build an MSP reply packet
    mspPacket_t packet;
    packet.reset();
    packet.makeResponse();
    packet.flags = 0;
    packet.function = 1;
    packet.addByte(1);

    // Mock out the serial port using a string stream
    std::string buf;
    StringStream ss(buf);

    // Ask the MSP class to send the packet to the stream
    bool result = MSPProtocol.sendPacket(&packet, &ss);
    TEST_ASSERT_EQUAL(true, result);    // success?

    // Assert that each byte sent matches expected
    TEST_ASSERT_EQUAL('$', buf[0]);
    TEST_ASSERT_EQUAL('X', buf[1]);
    TEST_ASSERT_EQUAL('>', buf[2]);
    TEST_ASSERT_EQUAL(0, buf[3]);       // flags
    TEST_ASSERT_EQUAL(1, buf[4]);       // lower byte of function
    TEST_ASSERT_EQUAL(0, buf[5]);       // upper byte of function
    TEST_ASSERT_EQUAL(1, buf[6]);       // lower byte of payload size
    TEST_ASSERT_EQUAL(0, buf[7]);       // upper byte of payload size
    TEST_ASSERT_EQUAL(1, buf[8]);       // payload
    TEST_ASSERT_EQUAL(224, (uint8_t)buf[9]);     // crc
}

extern void test_encapsulated_msp_send(void);
extern void test_encapsulated_msp_send_too_long(void);

// Unity setup/teardown
void setUp() {}
void tearDown() {}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_msp_receive);
    RUN_TEST(test_msp_send);

    RUN_TEST(test_encapsulated_msp_send);
    RUN_TEST(test_encapsulated_msp_send_too_long);

    UNITY_END();

    return 0;
}
