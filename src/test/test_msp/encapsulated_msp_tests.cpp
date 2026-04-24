#include <unity.h>
#include "msp.h"
#include "msptypes.h"
#include "mock_serial.h"

#include "devCRSF.h"

// Create a CRSF object to test
CRSF crsf;

void test_encapsulated_msp_send(void)
{
    // TEST CASE:
    // GIVEN the CRSF class has been instantiated using a mock UART
    // WHEN the crsf.sendMSPFrameToFC() function is called with a valid mspPacket_t MSP command to change the VTX to F1
    // THEN the mspPacket_t will be transcoded into an embedded crsf msp packet
    // AND the transcoded packet will be sent to the Stream object associated with the CRSF class

    // Make sure no msp messages are in the fifo
    crsf.ResetMspQueue();

    // Build an MSP packet with the MSP_SET_VTX_CONFIG cmd
    mspPacket_t packet;
    packet.reset();
    packet.makeCommand();
    packet.flags = 0;
    packet.function = 0x59; // MSP_SET_VTX_CONFIG
    packet.addByte(0x18);   // change to band 4 channel 1
    packet.addByte(0x00);   // second byte of band/channel
    packet.addByte(0x01);   // change to power at index 1
    packet.addByte(0x00);   // don't enable pitmode

    // Ask the CRSF class to send the encapsulated packet to the stream
    crsf.AddMspMessage(&packet);

    uint8_t* data;
    uint8_t len;
    crsf.GetMspMessage(&data, &len);

    // Assert that the correct number of total bytes were sent to the stream
    TEST_ASSERT_NOT_EQUAL(NULL, data);
    TEST_ASSERT_EQUAL(14, len);

    // Assert that each byte sent to the stream matches expected
    TEST_ASSERT_EQUAL(CRSF_ADDRESS_BROADCAST, data[0]);                  // device_addr
    TEST_ASSERT_EQUAL(12, data[1]);                                      // frame_size
    TEST_ASSERT_EQUAL(CRSF_FRAMETYPE_MSP_WRITE, data[2]);                // type
    TEST_ASSERT_EQUAL(CRSF_ADDRESS_FLIGHT_CONTROLLER, (uint8_t)data[3]); // dest_addr
    TEST_ASSERT_EQUAL(CRSF_ADDRESS_RADIO_TRANSMITTER, (uint8_t)data[4]); // orig_addr
    TEST_ASSERT_EQUAL(0x30, data[5]);                                    // header
    TEST_ASSERT_EQUAL(0x04, data[6]);                                    // mspPayloadSize
    TEST_ASSERT_EQUAL(packet.function, (uint8_t)data[7]);                // packet->cmd
    TEST_ASSERT_EQUAL(0x18, data[8]);                                    // newFrequency b1
    TEST_ASSERT_EQUAL(0x00, data[9]);                                    // newFrequency b2
    TEST_ASSERT_EQUAL(0x01, data[10]);                                   // newFrequency b2
    TEST_ASSERT_EQUAL(0x00, data[11]);                                   // pitmode
    TEST_ASSERT_EQUAL(0x44, data[12]);                                   // msp crc
    TEST_ASSERT_EQUAL(0x5E, data[13]);                                   // crsf crc
}

void test_encapsulated_msp_send_too_long(void)
{
    // TEST CASE:
    // GIVEN the CRSF class has been instantiated using a mock UART
    // WHEN the crsf.sendMSPFrameToFC() function is called with a invalid mspPacket_t MSP command (payload too long)
    // THEN the mspPacket_t will NOT be transcoded into an embedded crsf msp packet
    // AND nothing will be sent to the stream

    // Make sure no msp messages are in the fifo
    crsf.ResetMspQueue();

    // Build an MSP packet with a payload that is too long to send (>4 bytes)
    mspPacket_t packet;
    packet.reset();
    packet.makeCommand();
    packet.flags = 0;
    packet.function = 0x01;
    packet.addByte(0x01);
    packet.addByte(0x02);
    packet.addByte(0x03);
    packet.addByte(0x04);
    packet.addByte(0x05);

    // Ask the CRSF class to send the encapsulated packet to the stream
    crsf.AddMspMessage(&packet);

    uint8_t* data;
    uint8_t len;
    crsf.GetMspMessage(&data, &len);

    // Assert that nothing was sent to the stream
    TEST_ASSERT_EQUAL(NULL, data);
    TEST_ASSERT_EQUAL(0, len);
}
