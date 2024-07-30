#include <cstdint>
#include <iostream>
#include <unity.h>

#include "common.h"
#include "CRSF.h"
#include "msptypes.h"

using namespace std;

#if defined(MIXER)
uint32_t ChannelData[CRSF_NUM_CHANNELS + GYRO_SOURCES];      // Current state of channels, CRSF format
#else
uint32_t ChannelData[CRSF_NUM_CHANNELS];      // Current state of channels, CRSF format
#endif

GENERIC_CRC8 test_crc(CRSF_CRC_POLY);

void test_msp_simple_request(void)
{
    uint8_t vtxConfig[MSP_REQUEST_LENGTH(0)];

    TEST_ASSERT_EQUAL(7, MSP_REQUEST_PAYLOAD_LENGTH(0));
    TEST_ASSERT_EQUAL(13, MSP_REQUEST_LENGTH(0));

    CRSF::SetMspV2Request(vtxConfig, MSP_VTX_CONFIG, nullptr, 0);
    CRSF::SetExtendedHeaderAndCrc(vtxConfig, CRSF_FRAMETYPE_MSP_REQ, MSP_REQUEST_FRAME_SIZE(0), CRSF_ADDRESS_CRSF_RECEIVER, CRSF_ADDRESS_FLIGHT_CONTROLLER);

    crsf_ext_header_t *header = (crsf_ext_header_t *) vtxConfig;

    TEST_ASSERT_EQUAL(MSP_REQUEST_FRAME_SIZE(0), header->frame_size);
    TEST_ASSERT_EQUAL(CRSF_FRAMETYPE_MSP_REQ, header->type);
    TEST_ASSERT_EQUAL(CRSF_ADDRESS_FLIGHT_CONTROLLER, header->dest_addr);
    TEST_ASSERT_EQUAL(CRSF_ADDRESS_FLIGHT_CONTROLLER, header->device_addr);
    TEST_ASSERT_EQUAL(CRSF_ADDRESS_CRSF_RECEIVER, header->orig_addr);
    TEST_ASSERT_EQUAL(11, header->frame_size);

    uint8_t *data = vtxConfig + sizeof(crsf_ext_header_t);
    uint8_t compare [] = {0x50, 0, MSP_VTX_CONFIG, 0, 0, 0};

    TEST_ASSERT_EQUAL_INT8_ARRAY(compare, data, sizeof(compare));

    TEST_ASSERT_EQUAL(test_crc.calc(&vtxConfig[2], MSP_REQUEST_LENGTH(0) - 3), vtxConfig[MSP_REQUEST_LENGTH(0) - 1]);
}

void test_msp_clear_vtx_table_request(void)
{
    uint8_t payloadLength = 15;
    uint8_t payload[15] = {0, 0, 3, 0, 0, 0, 0, 4, 4, 0, 0, 6, 8, 5, 1}; // clear VTX table request
    uint8_t vtxConfig[MSP_REQUEST_LENGTH(payloadLength)];

    TEST_ASSERT_EQUAL(22, MSP_REQUEST_PAYLOAD_LENGTH(payloadLength));
    TEST_ASSERT_EQUAL(28, MSP_REQUEST_LENGTH(payloadLength));

    CRSF::SetMspV2Request(vtxConfig, MSP_SET_VTX_CONFIG, payload, payloadLength);
    CRSF::SetExtendedHeaderAndCrc(vtxConfig, CRSF_FRAMETYPE_MSP_REQ, MSP_REQUEST_FRAME_SIZE(payloadLength), CRSF_ADDRESS_CRSF_RECEIVER, CRSF_ADDRESS_FLIGHT_CONTROLLER);

    crsf_ext_header_t *header = (crsf_ext_header_t *) vtxConfig;

    TEST_ASSERT_EQUAL(MSP_REQUEST_FRAME_SIZE(payloadLength), header->frame_size);
    TEST_ASSERT_EQUAL(CRSF_FRAMETYPE_MSP_REQ, header->type);
    TEST_ASSERT_EQUAL(CRSF_ADDRESS_FLIGHT_CONTROLLER, header->dest_addr);
    TEST_ASSERT_EQUAL(CRSF_ADDRESS_FLIGHT_CONTROLLER, header->device_addr);
    TEST_ASSERT_EQUAL(CRSF_ADDRESS_CRSF_RECEIVER, header->orig_addr);
    TEST_ASSERT_EQUAL(26, header->frame_size);

    uint8_t *data = vtxConfig + sizeof(crsf_ext_header_t);
    uint8_t compare [] = {0x50, 0, MSP_SET_VTX_CONFIG, 0, payloadLength, 0, 0, 0, 3, 0, 0, 0, 0, 4, 4, 0, 0, 6, 8, 5, 1};

    TEST_ASSERT_EQUAL_INT8_ARRAY(compare, data, sizeof(compare));

    TEST_ASSERT_EQUAL(test_crc.calc(&vtxConfig[2], MSP_REQUEST_LENGTH(payloadLength) - 3), vtxConfig[MSP_REQUEST_LENGTH(payloadLength) - 1]);
}

// Unity setup/teardown
void setUp() {}
void tearDown() {}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_msp_simple_request);
    RUN_TEST(test_msp_clear_vtx_table_request);
    UNITY_END();

    return 0;
}