#include <cstdint>
#include <telemetry.h>
#include <unity.h>

Telemetry telemetry;
uint32_t ChannelData[CRSF_NUM_CHANNELS];      // Current state of channels, CRSF format

int sendData(uint8_t *data, int length)
{
    for(int i = 0; i < length; i++)
    {
        if (!telemetry.RXhandleUARTin(data[i]))
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
        telemetry.RXhandleUARTin(data[i]);
    }

    return length;
}

void test_function_battery(void)
{
    telemetry.ResetState();
    uint8_t batterySequence[] = {0xEC,10,CRSF_FRAMETYPE_BATTERY_SENSOR,0,0,0,0,0,0,0,0,109};
    uint8_t batterySequence2[] = {0xEC,10,CRSF_FRAMETYPE_BATTERY_SENSOR,1,0,0,0,0,0,0,0,46};
    int length = sizeof(batterySequence);
    int sentLength = sendData(batterySequence, length);
    TEST_ASSERT_EQUAL(length, sentLength);

    uint8_t* data;
    uint8_t receivedLength;
    telemetry.GetNextPayload(&receivedLength, &data);
    TEST_ASSERT_NOT_EQUAL(0, data);
    for (int i = 0; i < length; i++)
    {
        TEST_ASSERT_EQUAL(batterySequence[i], data[i]);
    }

    // simulate sending done + send another message of the same type to make sure that the repeated sending of only one type works

    // this unlocks the data but does not send it again since it's not updated
    TEST_ASSERT_EQUAL(false, telemetry.GetNextPayload(&receivedLength, &data));

    // update data
    sentLength = sendData(batterySequence2, length);
    TEST_ASSERT_EQUAL(length, sentLength);

    // now it's ready to be sent
    telemetry.GetNextPayload(&receivedLength, &data);
    TEST_ASSERT_NOT_EQUAL(0, data);

    for (int i = 0; i < length; i++)
    {
        TEST_ASSERT_EQUAL(batterySequence2[i], data[i]);
    }
}

void test_function_replace_old(void)
{
    telemetry.ResetState();
    uint8_t batterySequence[] =  {0xEC,10,CRSF_FRAMETYPE_BATTERY_SENSOR,0,0,0,0,0,0,0,0,109};
    uint8_t batterySequence2[] = {0xEC,10,CRSF_FRAMETYPE_BATTERY_SENSOR,1,0,0,0,0,0,0,0,46};

    sendDataWithoutCheck(batterySequence, sizeof(batterySequence));

    int length = sizeof(batterySequence2);
    int sentLength = sendData(batterySequence2, length);
    TEST_ASSERT_EQUAL(length, sentLength);

    uint8_t* data;
    uint8_t receivedLength;
    telemetry.GetNextPayload(&receivedLength, &data);
    TEST_ASSERT_NOT_EQUAL(0, data);
    for (int i = 0; i < length; i++)
    {
        TEST_ASSERT_EQUAL(batterySequence2[i], data[i]);
    }
}

void test_function_do_not_replace_old_locked(void)
{
    telemetry.ResetState();
    uint8_t batterySequence[] =  {0xEC,10, CRSF_FRAMETYPE_BATTERY_SENSOR,0,0,0,0,0,0,0,0,109};
    uint8_t batterySequence2[] = {0xEC,10, CRSF_FRAMETYPE_BATTERY_SENSOR,1,0,0,0,0,0,0,0,46};

    sendDataWithoutCheck(batterySequence, sizeof(batterySequence));
    uint8_t* data;
    uint8_t receivedLength;
    telemetry.GetNextPayload(&receivedLength, &data);
    sendDataWithoutCheck(batterySequence2, sizeof(batterySequence2));
    TEST_ASSERT_EQUAL(1, telemetry.UpdatedPayloadCount());
    TEST_ASSERT_NOT_EQUAL(0, data);
    for (int i = 0; i < sizeof(batterySequence); i++)
    {
        TEST_ASSERT_EQUAL(batterySequence[i], data[i]);
    }
}

void test_function_add_type(void)
{
    telemetry.ResetState();
    uint8_t batterySequence[] = {0xEC,10, CRSF_FRAMETYPE_BATTERY_SENSOR,0,0,0,0,0,0,0,0,109};
    uint8_t attitudeSequence[] = {0xEC,8, CRSF_FRAMETYPE_ATTITUDE,0,0,0,0,0,0,48};

    sendData(batterySequence, sizeof(batterySequence));

    int length = sizeof(attitudeSequence);
    int sentLength = sendData(attitudeSequence, length);
    TEST_ASSERT_EQUAL(length, sentLength);

    TEST_ASSERT_EQUAL(2, telemetry.UpdatedPayloadCount());

    uint8_t* data;
    uint8_t receivedLength;
    telemetry.GetNextPayload(&receivedLength, &data);

    telemetry.GetNextPayload(&receivedLength, &data);
    TEST_ASSERT_NOT_EQUAL(0, data);
    for (int i = 0; i < length; i++)
    {
        TEST_ASSERT_EQUAL(attitudeSequence[i], data[i]);
    }
}

void test_function_recover_from_junk(void)
{
    telemetry.ResetState();
    uint8_t bootloaderSequence[] = {
        0XEC,0xFF,100,10,10,
        0XEC,4,100,10,10,4,
        0xEC,0x04,CRSF_FRAMETYPE_COMMAND,0x62,0x6c,0x0A};
    int length = sizeof(bootloaderSequence);
    int sentLength = sendDataWithoutCheck(bootloaderSequence, length);
    TEST_ASSERT_EQUAL(true, telemetry.ShouldCallBootloader());
}

void test_function_bootloader_called(void)
{
    telemetry.ResetState();
    uint8_t bootloaderSequence[] = {0xEC,0x04,CRSF_FRAMETYPE_COMMAND,0x62,0x6c,0x0A};
    int length = sizeof(bootloaderSequence);
    int sentLength = sendData(bootloaderSequence, length);
    TEST_ASSERT_EQUAL(length, sentLength);
    TEST_ASSERT_EQUAL(true, telemetry.ShouldCallBootloader());
}

void test_function_store_unknown_type(void)
{
    telemetry.ResetState();
    uint8_t unknownSequence[] = {0xEC,0x04,CRSF_FRAMETYPE_PARAMETER_READ,0x62,0x6c,85};
    int length = sizeof(unknownSequence);
    int sentLength = sendData(unknownSequence, length);
    TEST_ASSERT_EQUAL(length, sentLength);
    TEST_ASSERT_EQUAL(1, telemetry.UpdatedPayloadCount());
}

void test_function_store_unknown_type_two_slots(void)
{
    telemetry.ResetState();
    uint8_t unknownSequence[] = {0xEC,0x04,CRSF_FRAMETYPE_PARAMETER_READ,0x62,0x6c,85};
    int length = sizeof(unknownSequence);
    int sentLength = sendData(unknownSequence, length);
    TEST_ASSERT_EQUAL(length, sentLength);

    uint8_t* data;
    uint8_t receivedLength;
    telemetry.GetNextPayload(&receivedLength, &data);

    sentLength = sendData(unknownSequence, length);
    TEST_ASSERT_EQUAL(length, sentLength);

    TEST_ASSERT_EQUAL(1, telemetry.UpdatedPayloadCount());
}

void test_function_store_ardupilot_status_text(void)
{
    telemetry.ResetState();
    uint8_t statusSequence[] = {0xEC,0x04,CRSF_FRAMETYPE_ARDUPILOT_RESP,CRSF_AP_CUSTOM_TELEM_STATUS_TEXT,0x6c,60};
    uint8_t otherSequence[] = {0xEC,0x04,CRSF_FRAMETYPE_ARDUPILOT_RESP,CRSF_AP_CUSTOM_TELEM_SINGLE_PACKET_PASSTHROUGH,0x6c,55};

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
    telemetry.ResetState();
    TEST_ASSERT_EQUAL(true, telemetry.RXhandleUARTin(0xEC));
}

void test_function_add_type_with_zero_crc(void)
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

    uint8_t* data;
    uint8_t receivedLength;
    telemetry.GetNextPayload(&receivedLength, &data);
    TEST_ASSERT_NOT_EQUAL(0, data);
    for (int i = 0; i < sizeof(sequence); i++)
    {
        TEST_ASSERT_EQUAL(sequence[i], data[i]);
    }
}

// Unity setup/teardown
void setUp() {}
void tearDown() {}

int main(int argc, char **argv)
{
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
    UNITY_END();

    return 0;
}
