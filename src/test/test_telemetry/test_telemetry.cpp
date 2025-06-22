#include <cstdint>
#include <unity.h>

#include <telemetry.h>
#include "common.h"
#include "CRSFRouter.h"

Telemetry telemetry;
uint32_t ChannelData[CRSF_NUM_CHANNELS];      // Current state of channels, CRSF format

class MockEndpoint : public CRSFEndpoint
{
public:
    MockEndpoint() : CRSFEndpoint((crsf_addr_e)1) {}
    void handleMessage(const crsf_header_t *message) override {}
} endpoint;

class MockConnector : public CRSFConnector {
public:
    void forwardMessage(const crsf_header_t *message) override {
        for (int i=0 ; i<message->frame_size + CRSF_FRAME_NOT_COUNTED_BYTES ; i++)
        {
            data.push_back(((uint8_t*)message)[i]);
        }
        telemetry.AppendTelemetryPackage((uint8_t*)message);
    }
    std::vector<uint8_t> data;
} connector;

CRSFRouter crsfRouter;

int sendData(uint8_t *data, int length)
{
    for(int i = 0; i < length; i++)
    {
        if (!telemetry.RXhandleUARTin(nullptr, data[i]))
        {
            return i;
        }
    }

    return length;
}

int sendDataWithoutCheck(uint8_t *data, int length)
{
    for(int i = 0; i < length; i++)
    {
        telemetry.RXhandleUARTin(nullptr, data[i]);
    }

    return length;
}

void test_function_battery(void)
{
    uint8_t batterySequence[] = {CRSF_ADDRESS_CRSF_RECEIVER,10,CRSF_FRAMETYPE_BATTERY_SENSOR,0,0,0,0,0,0,0,0,109};
    uint8_t batterySequence2[] = {CRSF_ADDRESS_CRSF_RECEIVER,10,CRSF_FRAMETYPE_BATTERY_SENSOR,1,0,0,0,0,0,0,0,46};
    int length = sizeof(batterySequence);
    int sentLength = sendData(batterySequence, length);
    TEST_ASSERT_EQUAL(length, sentLength);

    uint8_t data[CRSF_MAX_PACKET_LEN];
    uint8_t receivedLength;
    bool hasData = telemetry.GetNextPayload(&receivedLength, data);
    TEST_ASSERT_TRUE(hasData);
    for (int i = 0; i < length; i++)
    {
        TEST_ASSERT_EQUAL(batterySequence[i], data[i]);
    }

    // simulate sending done + send another message of the same type to make sure that the repeated sending of only one type works

    // this unlocks the data but does not send it again since it's not updated
    TEST_ASSERT_EQUAL(false, telemetry.GetNextPayload(&receivedLength, data));

    // update data
    sentLength = sendData(batterySequence2, length);
    TEST_ASSERT_EQUAL(length, sentLength);

    // now it's ready to be sent
    hasData = telemetry.GetNextPayload(&receivedLength, data);
    TEST_ASSERT_TRUE(hasData);

    for (int i = 0; i < length; i++)
    {
        TEST_ASSERT_EQUAL(batterySequence2[i], data[i]);
    }
}

void test_function_replace_old(void)
{
    uint8_t batterySequence[] =  {CRSF_ADDRESS_CRSF_RECEIVER,10,CRSF_FRAMETYPE_BATTERY_SENSOR,0,0,0,0,0,0,0,0,109};
    uint8_t batterySequence2[] = {CRSF_ADDRESS_CRSF_RECEIVER,10,CRSF_FRAMETYPE_BATTERY_SENSOR,1,0,0,0,0,0,0,0,46};

    sendDataWithoutCheck(batterySequence, sizeof(batterySequence));

    int length = sizeof(batterySequence2);
    int sentLength = sendData(batterySequence2, length);
    TEST_ASSERT_EQUAL(length, sentLength);

    uint8_t data[CRSF_MAX_PACKET_LEN];
    uint8_t receivedLength;
    bool hasData = telemetry.GetNextPayload(&receivedLength, data);
    TEST_ASSERT_TRUE(hasData);

    for (int i = 0; i < length; i++)
    {
        TEST_ASSERT_EQUAL(batterySequence2[i], data[i]);
    }
}

void test_function_do_not_replace_old_locked(void)
{
    uint8_t batterySequence[] =  {CRSF_ADDRESS_CRSF_RECEIVER,10, CRSF_FRAMETYPE_BATTERY_SENSOR,0,0,0,0,0,0,0,0,109};
    uint8_t batterySequence2[] = {CRSF_ADDRESS_CRSF_RECEIVER,10, CRSF_FRAMETYPE_BATTERY_SENSOR,1,0,0,0,0,0,0,0,46};

    sendDataWithoutCheck(batterySequence, sizeof(batterySequence));
    uint8_t data[CRSF_MAX_PACKET_LEN];
    uint8_t receivedLength;
    bool hasData = telemetry.GetNextPayload(&receivedLength, data);
    sendDataWithoutCheck(batterySequence2, sizeof(batterySequence2));
    TEST_ASSERT_EQUAL(1, telemetry.UpdatedPayloadCount());
    TEST_ASSERT_TRUE(hasData);
    for (int i = 0; i < sizeof(batterySequence); i++)
    {
        TEST_ASSERT_EQUAL(batterySequence[i], data[i]);
    }
}

void test_function_add_type(void)
{
    uint8_t batterySequence[] = {CRSF_ADDRESS_CRSF_RECEIVER,10, CRSF_FRAMETYPE_BATTERY_SENSOR,0,0,0,0,0,0,0,0,109};
    uint8_t attitudeSequence[] = {CRSF_ADDRESS_CRSF_RECEIVER,8, CRSF_FRAMETYPE_ATTITUDE,0,0,0,0,0,0,48};

    sendData(batterySequence, sizeof(batterySequence));

    int length = sizeof(attitudeSequence);
    int sentLength = sendData(attitudeSequence, length);
    TEST_ASSERT_EQUAL(length, sentLength);

    TEST_ASSERT_EQUAL(2, telemetry.UpdatedPayloadCount());

    uint8_t data[CRSF_MAX_PACKET_LEN];
    uint8_t receivedLength;
    telemetry.GetNextPayload(&receivedLength, data);

    bool hasData = telemetry.GetNextPayload(&receivedLength, data);
    TEST_ASSERT_TRUE(hasData);
    for (int i = 0; i < length; i++)
    {
        TEST_ASSERT_EQUAL(attitudeSequence[i], data[i]);
    }
}

void test_function_recover_from_junk(void)
{
    uint8_t bootloaderSequence[] = {
        CRSF_ADDRESS_CRSF_RECEIVER,0xFF,100,10,10,
        CRSF_ADDRESS_CRSF_RECEIVER,4,100,10,10,4,
        CRSF_ADDRESS_CRSF_RECEIVER,0x04,CRSF_FRAMETYPE_COMMAND,0x62,0x6c,0x0A};
    int length = sizeof(bootloaderSequence);
    int sentLength = sendDataWithoutCheck(bootloaderSequence, length);
    TEST_ASSERT_EQUAL(6, connector.data.size());
}

void test_function_bootloader_called(void)
{
    uint8_t bootloaderSequence[] = {CRSF_ADDRESS_CRSF_RECEIVER,0x04,CRSF_FRAMETYPE_COMMAND,0x62,0x6c,0x0A};
    int length = sizeof(bootloaderSequence);
    int sentLength = sendData(bootloaderSequence, length);
    TEST_ASSERT_EQUAL(length, sentLength);
    TEST_ASSERT_EQUAL(6, connector.data.size());
}

void test_function_store_unknown_type(void)
{
    uint8_t unknownSequence[] = {CRSF_ADDRESS_CRSF_RECEIVER,0x04,CRSF_FRAMETYPE_PARAMETER_READ,0x62,0x6c,85};
    int length = sizeof(unknownSequence);
    int sentLength = sendData(unknownSequence, length);
    TEST_ASSERT_EQUAL_MESSAGE(length, sentLength, "wrong length sent");
    TEST_ASSERT_EQUAL_MESSAGE(1, telemetry.UpdatedPayloadCount(), "payloads");
}

void test_function_store_unknown_type_two_slots(void)
{
    uint8_t unknownSequence[] = {CRSF_ADDRESS_CRSF_RECEIVER,0x04,CRSF_FRAMETYPE_PARAMETER_READ,0x62,0x6c,85};
    int length = sizeof(unknownSequence);
    int sentLength = sendData(unknownSequence, length);
    TEST_ASSERT_EQUAL(length, sentLength);

    uint8_t data[CRSF_MAX_PACKET_LEN];
    uint8_t receivedLength;
    telemetry.GetNextPayload(&receivedLength, data);

    sentLength = sendData(unknownSequence, length);
    TEST_ASSERT_EQUAL(length, sentLength);

    TEST_ASSERT_EQUAL(1, telemetry.UpdatedPayloadCount());
}

void test_function_store_ardupilot_status_text(void)
{
    uint8_t statusSequence[] = {CRSF_ADDRESS_CRSF_RECEIVER,0x04,CRSF_FRAMETYPE_ARDUPILOT_RESP,CRSF_AP_CUSTOM_TELEM_STATUS_TEXT,0x6c,60};
    uint8_t otherSequence[] = {CRSF_ADDRESS_CRSF_RECEIVER,0x04,CRSF_FRAMETYPE_ARDUPILOT_RESP,CRSF_AP_CUSTOM_TELEM_SINGLE_PACKET_PASSTHROUGH,0x6c,55};

    int length = sizeof(otherSequence);
    int sentLength = sendData(otherSequence, length);
    TEST_ASSERT_EQUAL(length, sentLength);

    length = sizeof(statusSequence);
    sentLength = sendData(statusSequence, length);
    TEST_ASSERT_EQUAL(length, sentLength);

    TEST_ASSERT_EQUAL(2, telemetry.UpdatedPayloadCount());
}

void test_function_uart_in(void)
{
    TEST_ASSERT_EQUAL(true, telemetry.RXhandleUARTin(nullptr, CRSF_ADDRESS_CRSF_RECEIVER));
}

void test_function_add_type_with_zero_crc(void)
{
    uint8_t sequence[] = {
        0xee,                   // device addr
        32,                     // frame size
        0x29,                   // frame type
        0xee,                   // dest addr
        CRSF_ADDRESS_CRSF_RECEIVER,                   // source addr
        'R','a','d','i','o','M','s','t','r',' ','R','P','3', 0, // device name (nul terminated string)
        'E', 'L', 'R', 'S',     // serial no.
        0x00, 0x00, 0x00, 0x00, // hardware version
        0x00, 0x30, 0x00, 0x00, // software version
        0x07,                   // field count
        0x00,                   // parameter version
        0x00                    // CRC
    };

    telemetry.AppendTelemetryPackage(sequence);

    uint8_t data[CRSF_MAX_PACKET_LEN];
    uint8_t receivedLength;
    bool hasData = telemetry.GetNextPayload(&receivedLength, data);
    TEST_ASSERT_TRUE(hasData);
    for (int i = 0; i < sizeof(sequence); i++)
    {
        TEST_ASSERT_EQUAL(sequence[i], data[i]);
    }
}

void test_only_one_device_info(void)
{
    telemetry.ResetState();
    uint8_t sequence[] = {
        0xee,                   // device addr
        32,                     // frame size
        0x29,                   // frame type
        0xee,                   // dest addr
        0xec,                   // source addr
        'R','a','d','i','o','M','s','t','r',' ','R','P','3', 0, // device name (nul terminated string)
        'E', 'L', 'R', 'S',     // serial no.
        0x00, 0x00, 0x00, 0x00, // hardware version
        0x00, 0x30, 0x00, 0x00, // software version
        0x07,                   // field count
        0x00,                   // parameter version
        0x00                    // CRC
    };

    telemetry.AppendTelemetryPackage(sequence);
    telemetry.AppendTelemetryPackage(sequence);
    telemetry.AppendTelemetryPackage(sequence);
    telemetry.AppendTelemetryPackage(sequence);

    uint8_t data[CRSF_MAX_PACKET_LEN];
    uint8_t receivedLength;
    bool hasData = telemetry.GetNextPayload(&receivedLength, data);
    TEST_ASSERT_TRUE(hasData);
    hasData = telemetry.GetNextPayload(&receivedLength, data);
    TEST_ASSERT_FALSE(hasData);
}

void test_only_one_device_info_per_source(void)
{
    telemetry.ResetState();
    uint8_t sequenceEC[] = {
        0xee,                   // device addr
        32,                     // frame size
        0x29,                   // frame type
        0xee,                   // dest addr
        0xec,                   // source addr
        'R','a','d','i','o','M','s','t','r',' ','R','P','3', 0, // device name (nul terminated string)
        'E', 'L', 'R', 'S',     // serial no.
        0x00, 0x00, 0x00, 0x00, // hardware version
        0x00, 0x30, 0x00, 0x00, // software version
        0x07,                   // field count
        0x00,                   // parameter version
        0x00                    // CRC
    };
    uint8_t sequenceC8[] = {
        0xee,                   // device addr
        29,                     // frame size
        0x29,                   // frame type
        0xee,                   // dest addr
        0xc8,                   // source addr
        'B','e','t','a','f','l','i','g','h','t', 0, // device name (nul terminated string)
        0x00, 0x00, 0x00, 0x00, // serial no.
        0x00, 0x00, 0x00, 0x00, // hardware version
        0x00, 0x04, 0x05, 0x02, // software version
        0x00,                   // field count
        0x00,                   // parameter version
        0x00                    // CRC
    };

    telemetry.AppendTelemetryPackage(sequenceEC);
    TEST_ASSERT_EQUAL(1, telemetry.UpdatedPayloadCount());
    telemetry.AppendTelemetryPackage(sequenceC8);
    TEST_ASSERT_EQUAL(2, telemetry.UpdatedPayloadCount());
    telemetry.AppendTelemetryPackage(sequenceEC);
    TEST_ASSERT_EQUAL(2, telemetry.UpdatedPayloadCount());
    telemetry.AppendTelemetryPackage(sequenceC8);
    TEST_ASSERT_EQUAL(2, telemetry.UpdatedPayloadCount());
}

void test_prioritised_settings_entry_messages(void)
{
    telemetry.ResetState();
    uint8_t batterySequence[] = {0xEC,10,CRSF_FRAMETYPE_BATTERY_SENSOR,0,0,0,0,0,0,0,0,109};
    sendData(batterySequence, sizeof(batterySequence));

    uint8_t settingsSequence[] = {0xEC,0,0,0,0,0,0,0,0,0,0,0};
    crsfRouter.SetExtendedHeaderAndCrc((crsf_ext_header_t *)settingsSequence, CRSF_FRAMETYPE_PARAMETER_SETTINGS_ENTRY, sizeof(settingsSequence)-CRSF_FRAME_NOT_COUNTED_BYTES, CRSF_ADDRESS_CRSF_RECEIVER, CRSF_ADDRESS_RADIO_TRANSMITTER);
    sendData(settingsSequence, sizeof(settingsSequence));

    uint8_t payloadSize;
    uint8_t payload[CRSF_MAX_PACKET_LEN];
;

    telemetry.GetNextPayload(&payloadSize, payload);
    TEST_ASSERT_EQUAL(CRSF_FRAMETYPE_PARAMETER_SETTINGS_ENTRY, payload[CRSF_TELEMETRY_TYPE_INDEX]);

    telemetry.GetNextPayload(&payloadSize, payload);
    TEST_ASSERT_EQUAL(CRSF_FRAMETYPE_BATTERY_SENSOR, payload[CRSF_TELEMETRY_TYPE_INDEX]);
}

// Unity setup/teardown
void setUp()
{
    telemetry.ResetState();
    connector.data.clear();
}

void tearDown()
{
}

int main(int argc, char **argv)
{
    crsfRouter.addEndpoint(&endpoint);
    crsfRouter.addConnector(&connector);
    connector.addDevice(CRSF_ADDRESS_CRSF_RECEIVER);

    UNITY_BEGIN();
    RUN_TEST(test_function_uart_in);
    RUN_TEST(test_function_bootloader_called);
    RUN_TEST(test_function_battery);
    RUN_TEST(test_function_replace_old);
    RUN_TEST(test_function_do_not_replace_old_locked);
    RUN_TEST(test_function_add_type);
    RUN_TEST(test_function_recover_from_junk);
    RUN_TEST(test_function_store_unknown_type);
    RUN_TEST(test_function_store_unknown_type_two_slots);
    RUN_TEST(test_function_store_ardupilot_status_text);
    RUN_TEST(test_function_add_type_with_zero_crc);
    RUN_TEST(test_only_one_device_info);
    RUN_TEST(test_only_one_device_info_per_source);
    RUN_TEST(test_prioritised_settings_entry_messages);
    UNITY_END();

    return 0;
}
