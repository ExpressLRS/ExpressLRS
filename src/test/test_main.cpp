#include <Arduino.h>
#include <unity.h>
#include <Stream.h>

#include "msp.h"

// Mock the serial port using a string stream class
// This will allow us to assert what gets sent on the serial port
class StringStream : public Stream
{
public:
    StringStream(String &s) : string(s), position(0) { }

    // Stream methods
    virtual int available() { return string.length() - position; }
    virtual int read() { return position < string.length() ? string[position++] : -1; }
    virtual int peek() { return position < string.length() ? string[position] : -1; }
    virtual void flush() { };
    // Print methods
    virtual size_t write(uint8_t c) { string += (char)c; return 1;};

private:
    String &string;
    unsigned int length;
    unsigned int position;
};

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
    String buf;
    StringStream ss(buf);

    // Ask the MSP class to send the packet to the stream
    bool result = MSPProtocol.sendPacket(packet, &ss);

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
    TEST_ASSERT_EQUAL(224, buf[9]);     // crc
    TEST_ASSERT_EQUAL(true, result);    // success?
}

void setup() {
    // NOTE!!! Wait for >2 secs
    // if board doesn't support software reset via Serial.DTR/RTS
    delay(2000);

    UNITY_BEGIN(); // IMPORTANT LINE!

    RUN_TEST(test_msp_receive);
    RUN_TEST(test_msp_send);
}

void loop() {
    UNITY_END(); // stop unit testing
}
