#include "common.h"
#include "mock_serial.h"
#include "msp.h"
#include <cstdint>
#include <iostream>
#include <string>
#include <unity.h>

MSP MSPProtocol;
MSP MSPOversizeProtocol;
MSP MSPMaxPayloadProtocol;
MSP MSPInvalidHeaderProtocol;
MSP MSPResponseProtocol;
MSP MSPUnknownTypeProtocol;
MSP MSPZeroPayloadProtocol;
MSP MSPCrcFailureProtocol;
MSP MSPMarkReceivedProtocol;
MSP MSPUnmarkedProtocol;
uint32_t ChannelData[CRSF_NUM_CHANNELS];      // Current state of channels, CRSF format

static bool feed_msp_bytes(MSP &protocol, const std::string &buf)
{
    bool packetComplete = false;
    for (const char c : buf)
    {
        packetComplete = protocol.processReceivedByte(c);
    }
    return packetComplete;
}

static bool write_msp_packet(mspPacket_t &packet, std::string &buf)
{
    StringStream ss(buf);
    return MSP::sendPacket(&packet, &ss);
}

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

void test_msp_receive_rejects_oversized_payload(void)
{
    const uint8_t header[] = {'$', 'X', '<', 0, 1, 0, MSP_PORT_INBUF_SIZE + 1, 0};
    bool packetComplete = false;
    for (const uint8_t c : header)
    {
        packetComplete = MSPOversizeProtocol.processReceivedByte(c);
        TEST_ASSERT_FALSE(packetComplete);
    }

    for (uint8_t i = 0; i < MSP_PORT_INBUF_SIZE + 2; ++i)
    {
        packetComplete = MSPOversizeProtocol.processReceivedByte('A');
        TEST_ASSERT_FALSE(packetComplete);
    }

    mspPacket_t validPacket;
    validPacket.reset();
    validPacket.makeCommand();
    validPacket.flags = 0;
    validPacket.function = 1;
    validPacket.addByte('B');

    std::string buf;
    StringStream ss(buf);
    TEST_ASSERT_TRUE(MSP::sendPacket(&validPacket, &ss));
    TEST_ASSERT_TRUE(feed_msp_bytes(MSPOversizeProtocol, buf));
}

void test_msp_receive_accepts_max_payload(void)
{
    mspPacket_t packet;
    packet.reset();
    packet.makeCommand();
    packet.flags = 0;
    packet.function = 1;
    for (uint8_t i = 0; i < MSP_PORT_INBUF_SIZE; ++i)
    {
        packet.addByte(i);
    }

    std::string buf;
    StringStream ss(buf);
    TEST_ASSERT_TRUE(MSP::sendPacket(&packet, &ss));
    TEST_ASSERT_TRUE(feed_msp_bytes(MSPMaxPayloadProtocol, buf));

    mspPacket_t *received = MSPMaxPayloadProtocol.getReceivedPacket();
    TEST_ASSERT_EQUAL(MSP_PORT_INBUF_SIZE, received->payloadSize);
    TEST_ASSERT_EQUAL(0, received->payload[0]);
    TEST_ASSERT_EQUAL(MSP_PORT_INBUF_SIZE - 1, received->payload[MSP_PORT_INBUF_SIZE - 1]);
}

void test_msp_receive_recovers_from_invalid_header_start(void)
{
    TEST_ASSERT_FALSE(MSPInvalidHeaderProtocol.processReceivedByte('$'));
    TEST_ASSERT_FALSE(MSPInvalidHeaderProtocol.processReceivedByte('M'));

    mspPacket_t packet;
    packet.reset();
    packet.makeCommand();
    packet.flags = 0;
    packet.function = 1;
    packet.addByte('C');

    std::string buf;
    TEST_ASSERT_TRUE(write_msp_packet(packet, buf));
    TEST_ASSERT_TRUE(feed_msp_bytes(MSPInvalidHeaderProtocol, buf));
}

void test_msp_receive_accepts_response_packets(void)
{
    mspPacket_t packet;
    packet.reset();
    packet.makeResponse();
    packet.flags = 0;
    packet.function = 1;
    packet.addByte(7);

    std::string buf;
    TEST_ASSERT_TRUE(write_msp_packet(packet, buf));
    TEST_ASSERT_TRUE(feed_msp_bytes(MSPResponseProtocol, buf));

    mspPacket_t *received = MSPResponseProtocol.getReceivedPacket();
    TEST_ASSERT_EQUAL(MSP_PACKET_RESPONSE, received->type);
    TEST_ASSERT_EQUAL(7, received->payload[0]);
}

void test_msp_receive_ignores_unknown_packet_type(void)
{
    TEST_ASSERT_FALSE(MSPUnknownTypeProtocol.processReceivedByte('$'));
    TEST_ASSERT_FALSE(MSPUnknownTypeProtocol.processReceivedByte('X'));
    TEST_ASSERT_FALSE(MSPUnknownTypeProtocol.processReceivedByte('?'));

    mspPacket_t packet;
    packet.reset();
    packet.makeCommand();
    packet.flags = 0;
    packet.function = 1;
    packet.addByte('D');

    std::string buf;
    TEST_ASSERT_TRUE(write_msp_packet(packet, buf));
    TEST_ASSERT_TRUE(feed_msp_bytes(MSPUnknownTypeProtocol, buf));
}

void test_msp_receive_accepts_zero_payload_command(void)
{
    mspPacket_t packet;
    packet.reset();
    packet.makeCommand();
    packet.flags = 0;
    packet.function = 2;

    std::string buf;
    TEST_ASSERT_TRUE(write_msp_packet(packet, buf));
    TEST_ASSERT_TRUE(feed_msp_bytes(MSPZeroPayloadProtocol, buf));

    mspPacket_t *received = MSPZeroPayloadProtocol.getReceivedPacket();
    TEST_ASSERT_EQUAL(MSP_PACKET_COMMAND, received->type);
    TEST_ASSERT_EQUAL(0, received->payloadSize);
}

void test_msp_receive_rejects_bad_crc_and_recovers(void)
{
    mspPacket_t packet;
    packet.reset();
    packet.makeCommand();
    packet.flags = 0;
    packet.function = 1;
    packet.addByte('E');

    std::string badCrcBuf;
    TEST_ASSERT_TRUE(write_msp_packet(packet, badCrcBuf));
    badCrcBuf[badCrcBuf.size() - 1] ^= 0xff;
    TEST_ASSERT_FALSE(feed_msp_bytes(MSPCrcFailureProtocol, badCrcBuf));

    std::string validBuf;
    TEST_ASSERT_TRUE(write_msp_packet(packet, validBuf));
    TEST_ASSERT_TRUE(feed_msp_bytes(MSPCrcFailureProtocol, validBuf));
}

void test_msp_mark_packet_received_allows_next_packet(void)
{
    mspPacket_t firstPacket;
    firstPacket.reset();
    firstPacket.makeCommand();
    firstPacket.flags = 0;
    firstPacket.function = 1;
    firstPacket.addByte('F');

    std::string firstBuf;
    TEST_ASSERT_TRUE(write_msp_packet(firstPacket, firstBuf));
    TEST_ASSERT_TRUE(feed_msp_bytes(MSPMarkReceivedProtocol, firstBuf));

    MSPMarkReceivedProtocol.markPacketReceived();

    mspPacket_t secondPacket;
    secondPacket.reset();
    secondPacket.makeCommand();
    secondPacket.flags = 0;
    secondPacket.function = 1;
    secondPacket.addByte('G');

    std::string secondBuf;
    TEST_ASSERT_TRUE(write_msp_packet(secondPacket, secondBuf));
    TEST_ASSERT_TRUE(feed_msp_bytes(MSPMarkReceivedProtocol, secondBuf));
}

void test_msp_receive_recovers_if_not_marked_before_next_byte(void)
{
    mspPacket_t packet;
    packet.reset();
    packet.makeCommand();
    packet.flags = 0;
    packet.function = 1;
    packet.addByte('H');

    std::string buf;
    TEST_ASSERT_TRUE(write_msp_packet(packet, buf));
    TEST_ASSERT_TRUE(feed_msp_bytes(MSPUnmarkedProtocol, buf));
    TEST_ASSERT_FALSE(MSPUnmarkedProtocol.processReceivedByte('$'));
    TEST_ASSERT_TRUE(feed_msp_bytes(MSPUnmarkedProtocol, buf));
}

void test_msp_send_rejects_unknown_packet_type(void)
{
    mspPacket_t packet;
    packet.reset();

    std::string buf;
    TEST_ASSERT_FALSE(write_msp_packet(packet, buf));
    TEST_ASSERT_TRUE(buf.empty());
}

void test_msp_send_rejects_empty_response(void)
{
    mspPacket_t packet;
    packet.reset();
    packet.makeResponse();
    packet.flags = 0;
    packet.function = 1;

    std::string buf;
    TEST_ASSERT_FALSE(write_msp_packet(packet, buf));
    TEST_ASSERT_TRUE(buf.empty());
}

extern void test_encapsulated_msp_send(void);
extern void test_encapsulated_msp_send_not_too_long(void);
extern void test_encapsulated_msp_send_too_long(void);

// Unity setup/teardown
void setUp() {}
void tearDown() {}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_msp_receive);
    RUN_TEST(test_msp_send);
    RUN_TEST(test_msp_receive_rejects_oversized_payload);
    RUN_TEST(test_msp_receive_accepts_max_payload);
    RUN_TEST(test_msp_receive_recovers_from_invalid_header_start);
    RUN_TEST(test_msp_receive_accepts_response_packets);
    RUN_TEST(test_msp_receive_ignores_unknown_packet_type);
    RUN_TEST(test_msp_receive_accepts_zero_payload_command);
    RUN_TEST(test_msp_receive_rejects_bad_crc_and_recovers);
    RUN_TEST(test_msp_mark_packet_received_allows_next_packet);
    RUN_TEST(test_msp_receive_recovers_if_not_marked_before_next_byte);
    RUN_TEST(test_msp_send_rejects_unknown_packet_type);
    RUN_TEST(test_msp_send_rejects_empty_response);

    RUN_TEST(test_encapsulated_msp_send);
    RUN_TEST(test_encapsulated_msp_send_not_too_long);
    RUN_TEST(test_encapsulated_msp_send_too_long);

    UNITY_END();

    return 0;
}
