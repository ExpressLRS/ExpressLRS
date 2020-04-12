#include <Arduino.h>
#include <unity.h>
#include "msp.h"
#include "msptypes.h"
#include "mock_serial.h"
#include "CRSF.h"

CRSF CRSFProtocol;

void test_encapsulated_msp_send(void)
{
    // TEST CASE:
    // GIVEN an instance of the MSP class has been instantiated
    // WHEN a valid packet is passed as an argument to sendPacket()
    // THEN the complete packet will be transmitted to the passed in Stream object
    // AND the return value will be true
    
    // Build an MSP packet
    mspPacket_t packet;
    packet.reset();
    packet.makeCommand();
    packet.flags = 0;
    packet.function = 1;
    packet.addByte(1);

    // Mock out the serial port using a string stream
    String buf;
    StringStream ss(buf);

    // Ask the CRSF class to send the encapsulated packet to the stream
    

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