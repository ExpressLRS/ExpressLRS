#include <cstdint>
#include <telemetry.h>
#include <unity.h>

Telemetry telemetry;

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
    uint8_t batterySequence[] = {0xEC,10,0x08,0,0,0,0,0,0,0,0,109};
    int length = sizeof(batterySequence);
    int sentLength = sendData(batterySequence, length);
    TEST_ASSERT_EQUAL(length, sentLength);

    volatile crsf_telemetry_package_t* head = telemetry.GetPackageHead();
    TEST_ASSERT_NOT_EQUAL(0, head);
    TEST_ASSERT_EQUAL(0, head->next);

    for (int i = 0; i < length; i++)
    {
        TEST_ASSERT_EQUAL(batterySequence[i], head->data[i]);
    }
}

void test_function_replace_old(void)
{
    telemetry.ResetState();
    uint8_t batterySequence[] = {0xEC,10,0x08,0,0,0,0,0,0,0,0,109};
    uint8_t batterySequence2[] = {0xEC,10,0x08,1,0,0,0,0,0,0,0,46};

    sendDataWithoutCheck(batterySequence, sizeof(batterySequence));

    int length = sizeof(batterySequence2);
    int sentLength = sendData(batterySequence2, length);
    TEST_ASSERT_EQUAL(length, sentLength);

    volatile crsf_telemetry_package_t* head = telemetry.GetPackageHead();
    TEST_ASSERT_NOT_EQUAL(0, head);
    TEST_ASSERT_EQUAL(0, head->next);

    for (int i = 0; i < length; i++)
    {
        TEST_ASSERT_EQUAL(batterySequence2[i], head->data[i]);
    }
}

void test_function_do_not_replace_old_locked(void)
{
    telemetry.ResetState();
    uint8_t batterySequence[] = {0xEC,10,0x08,0,0,0,0,0,0,0,0,109};
    uint8_t batterySequence2[] = {0xEC,10,0x08,1,0,0,0,0,0,0,0,46};

    sendDataWithoutCheck(batterySequence, sizeof(batterySequence));
    telemetry.GetPackageHead();
    sendDataWithoutCheck(batterySequence2, sizeof(batterySequence2));

    TEST_ASSERT_EQUAL(1, telemetry.QueueLength());

    volatile crsf_telemetry_package_t* head = telemetry.GetPackageHead();
    TEST_ASSERT_NOT_EQUAL(0, head);
    TEST_ASSERT_EQUAL(0, head->next);

    for (int i = 0; i < sizeof(batterySequence); i++)
    {
        TEST_ASSERT_EQUAL(batterySequence[i], head->data[i]);
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

    TEST_ASSERT_EQUAL(2, telemetry.QueueLength());

    telemetry.DeleteHead();

    volatile crsf_telemetry_package_t* head = telemetry.GetPackageHead();
    TEST_ASSERT_NOT_EQUAL(0, head);
    TEST_ASSERT_EQUAL(0, head->next);

    for (int i = 0; i < length; i++)
    {
        TEST_ASSERT_EQUAL(attitudeSequence[i], head->data[i]);
    }
}

void test_function_recover_from_junk(void)
{
    telemetry.ResetState();
    uint8_t bootloaderSequence[] = {
        0XEC,0xFF,100,10,10,
        0XEC,4,100,10,10,4,
        0xEC,0x04,0x32,0x62,0x6c,0x0A};
    int length = sizeof(bootloaderSequence);
    int sentLength = sendDataWithoutCheck(bootloaderSequence, length);
    TEST_ASSERT_EQUAL(true, telemetry.callBootloader);
}

void test_function_bootloader_called(void)
{
    telemetry.ResetState();
    uint8_t bootloaderSequence[] = {0xEC,0x04,0x32,0x62,0x6c,0x0A};
    int length = sizeof(bootloaderSequence);
    int sentLength = sendData(bootloaderSequence, length);
    TEST_ASSERT_EQUAL(length, sentLength);
    TEST_ASSERT_EQUAL(true, telemetry.callBootloader);
}

void test_function_uart_in(void)
{
    telemetry.ResetState();
    TEST_ASSERT_EQUAL(true, telemetry.RXhandleUARTin(0xEC));
}

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
    UNITY_END();

    return 0;
}