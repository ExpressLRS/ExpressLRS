#include <cstdint>
#include <iostream>
#include <unity.h>
#include "../test_msp/mock_serial.h"

#include "CRSFRouter.h"
#include "common.h"
#include "lua.h"
#include "options.h"

using namespace std;

uint32_t ChannelData[CRSF_NUM_CHANNELS];      // Current state of channels, CRSF format

GENERIC_CRC8 test_crc(CRSF_CRC_POLY);

class MockEndpoint : public CRSFEndpoint
{
public:
    MockEndpoint() : CRSFEndpoint(CRSF_ADDRESS_CRSF_RECEIVER) {}
    void handleMessage(const crsf_header_t *message) override {}
} crsfEndpoint;

class MockConnector : public CRSFConnector {
public:
    void forwardMessage(const crsf_header_t *message) override {
        for (int i=0 ; i<message->frame_size + CRSF_FRAME_NOT_COUNTED_BYTES ; i++)
        {
            data.push_back(((uint8_t*)message)[i]);
        }
    }
    std::vector<uint8_t> data;
} connector;

CRSFRouter crsfRouter;

void test_ver_to_u32(void)
{
    constexpr struct tagVerItem {
        const char verStr[128];
        const uint32_t verU32;
    } VERSION_STRINGS[] = {
        {{108,117,97,45,102,111,108,100,101,114,45,117,112,100,97,116,101,32,73,83,77,50,71,52,0}, (OTA_VERSION_ID << 16)}, // lua-folder-update ISM2G4
        {{0x32, 0x2e, 0x32, 0x2e, 0x31, 0x35, 32,73,83,77,50,71,52,0}, 0x0002020f}, // 2.2.15 ISM2G4
        {{0x31, 0x2e, 0x32, 0x2e, 0x33, 0x2e, 0x34, 32,73,83,77,50,71,52,0}, 0x01020304}, // 1.2.3.4 ISM2G4
        {{0x31, 0x30, 0x30, 0x2e, 0x32, 0x35, 0x35, 32,0}, (OTA_VERSION_ID << 16)}, // 100.255(space)
        {"3.1.2",0x00030102},
        {"4.x.x-maint",0x00040000},
        {{0}, 0},
    };

    const struct tagVerItem *ver = VERSION_STRINGS;
    while (ver->verStr[0])
    {
        extern uint32_t VersionStrToU32(const char *verStr);

        TEST_ASSERT_EQUAL_HEX32(ver->verU32, VersionStrToU32(ver->verStr));
        ++ver;
    }
}

void test_device_info(void)
{
    TEST_ASSERT_EQUAL(22, DEVICE_INFORMATION_PAYLOAD_LENGTH);
    TEST_ASSERT_EQUAL(28, DEVICE_INFORMATION_LENGTH);

    luaParamUpdateReq(CRSF_ADDRESS_FLIGHT_CONTROLLER, 0, 0, 0);
    sendLuaDevicePacket(CRSF_ADDRESS_CRSF_RECEIVER);

    crsf_ext_header_t *header = (crsf_ext_header_t *) connector.data.data();

    TEST_ASSERT_EQUAL(26, header->frame_size);
    TEST_ASSERT_EQUAL(CRSF_FRAMETYPE_DEVICE_INFO, header->type);
    TEST_ASSERT_EQUAL(CRSF_ADDRESS_FLIGHT_CONTROLLER, header->dest_addr);
    TEST_ASSERT_EQUAL(CRSF_ADDRESS_FLIGHT_CONTROLLER, header->device_addr);
    TEST_ASSERT_EQUAL(CRSF_ADDRESS_CRSF_RECEIVER, header->orig_addr);
    TEST_ASSERT_EQUAL(DEVICE_INFORMATION_FRAME_SIZE, header->frame_size);

    uint8_t *data = connector.data.data() + sizeof(crsf_ext_header_t);
    uint8_t compare [] = {'t', 'e', 's', 't', 'i', 'n', 'g', 0x0, 0x45, 0x4c, 0x52, 0x53, 0x0, 0x0, 0x0, 0x0, 0x0, 1, 2, 3, 0x0, 0x0};

    TEST_ASSERT_EQUAL_INT8_ARRAY(compare, data, sizeof(compare));

    TEST_ASSERT_EQUAL(test_crc.calc(connector.data.data() + 2, DEVICE_INFORMATION_LENGTH-3), connector.data.data()[DEVICE_INFORMATION_LENGTH - 1]);
}

int main(int argc, char **argv)
{
    connector.addDevice(CRSF_ADDRESS_FLIGHT_CONTROLLER); // our connector sends to the FC
    crsfRouter.addConnector(&connector);
    crsfRouter.addEndpoint(&crsfEndpoint);

    UNITY_BEGIN();
    RUN_TEST(test_ver_to_u32);
    RUN_TEST(test_device_info);
    UNITY_END();

    return 0;
}
