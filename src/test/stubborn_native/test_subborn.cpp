#include <cstdint>
#include <stubborn_link.h>
#include <unity.h>


StubbornLink link;

void test_stubborn_link_sends_data(void)
{
    uint8_t batterySequence[] = {0xEC,10, 0x08,0,0,0,0,0,0,0,0,109};
    link.ResetState();
    link.SetDataToTransmit(sizeof(batterySequence), batterySequence);
    uint8_t *data;
    uint8_t maxLength;
    uint8_t packageIndex;

    for(int i = 0; i < sizeof(batterySequence); i++)
    {
        link.GetCurrentPayload(&packageIndex, &maxLength, &data);
        TEST_ASSERT_EQUAL(1, maxLength);
        TEST_ASSERT_EQUAL(i + 1, packageIndex);
        TEST_ASSERT_EQUAL(batterySequence[i], *data);
        link.ConfirmCurrentPayload();
    }
    link.GetCurrentPayload(&packageIndex, &maxLength, &data);
    TEST_ASSERT_EQUAL(0, maxLength);
}

void test_stubborn_link_sends_data_even_bytes_per_call(void)
{
    uint8_t batterySequence[] = {0xEC,10, 0x08,0,0,0,0,0,0,0,0,109};
    link.ResetState();
    link.SetBytesPerCall(2);
    link.SetDataToTransmit(sizeof(batterySequence), batterySequence);
    uint8_t *data;
    uint8_t maxLength;
    uint8_t packageIndex;

    for(int i = 0; i < sizeof(batterySequence) / 2; i++)
    {
        link.GetCurrentPayload(&packageIndex, &maxLength, &data);
        TEST_ASSERT_EQUAL(2, maxLength);
        TEST_ASSERT_EQUAL(batterySequence[i*2], data[0]);
        TEST_ASSERT_EQUAL(batterySequence[i*2+1], data[1]);
        link.ConfirmCurrentPayload();
    }
    link.GetCurrentPayload(&packageIndex, &maxLength, &data);
    TEST_ASSERT_EQUAL(0, maxLength);
}

void test_stubborn_link_sends_data_odd_bytes_per_call(void)
{
    uint8_t batterySequence[] = {0xEC,11, 0x08,0,0,0,0,0,0,0,109};
    link.ResetState();
    link.SetBytesPerCall(3);
    link.SetDataToTransmit(sizeof(batterySequence), batterySequence);
    uint8_t *data;
    uint8_t maxLength;
    uint8_t packageIndex;

    for(int i = 0; i < sizeof(batterySequence) / 3; i++)
    {
        link.GetCurrentPayload(&packageIndex, &maxLength, &data);
        TEST_ASSERT_EQUAL(3, maxLength);
        TEST_ASSERT_EQUAL(batterySequence[i*3], data[0]);
        TEST_ASSERT_EQUAL(batterySequence[i*3+1], data[1]);
        TEST_ASSERT_EQUAL(batterySequence[i*3+2], data[2]);
        link.ConfirmCurrentPayload();
    }
    link.GetCurrentPayload(&packageIndex, &maxLength, &data);
    TEST_ASSERT_EQUAL(2, maxLength);
    TEST_ASSERT_EQUAL(batterySequence[9], data[0]);
    TEST_ASSERT_EQUAL(batterySequence[10], data[1]);

    link.ConfirmCurrentPayload();
    link.GetCurrentPayload(&packageIndex, &maxLength, &data);
    TEST_ASSERT_EQUAL(0, maxLength);
}

void test_stubborn_link_sends_data_larger_frame_size(void)
{
    uint8_t batterySequence[] = {0xEC,10, 0x08,0,0,0,0,0,0,0,0,109};
    link.ResetState();
    link.SetBytesPerCall(13);
    link.SetDataToTransmit(sizeof(batterySequence), batterySequence);
    uint8_t *data;
    uint8_t maxLength;
    uint8_t packageIndex;

    link.GetCurrentPayload(&packageIndex, &maxLength, &data);
    TEST_ASSERT_EQUAL(12, maxLength);
    for(int i = 0; i < sizeof(batterySequence); i++)
    {
        TEST_ASSERT_EQUAL(batterySequence[i], data[i]);
    }

    link.ConfirmCurrentPayload();
    link.GetCurrentPayload(&packageIndex, &maxLength, &data);
    TEST_ASSERT_EQUAL(0, maxLength);
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_stubborn_link_sends_data);
    RUN_TEST(test_stubborn_link_sends_data_even_bytes_per_call);
    RUN_TEST(test_stubborn_link_sends_data_odd_bytes_per_call);
    RUN_TEST(test_stubborn_link_sends_data_larger_frame_size);
    UNITY_END();

    return 0;
}
