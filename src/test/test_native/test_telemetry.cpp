#include <telemetry.h>
#include <unity.h>

Telemetry telemetry;

int sendData(unsigned char *data, int length) 
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

void test_function_bootloader_called(void)
{
    telemetry.ResetState();
    unsigned char bootloaderSequence[] = {0xEC,0x04,0x32,0x62,0x6c,0x0A};
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
    UNITY_END();

    return 0;
}