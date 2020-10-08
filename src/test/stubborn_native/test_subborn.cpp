#include <cstdint>
#include <stubborn_link.h>
#include <unity.h>


StubbornLink link;

void test_stubborn_link_sends_data(void)
{
    uint8_t batterySequence[] = {0xEC,10, 0x08,0,0,0,0,0,0,0,0,109};
    link.ResetState();
    link.SetDataToTransmit(12, batterySequence);
    uint8_t *data;
    uint8_t maxLength;
    link.GetCurrentPayload(&maxLength, &data);
    TEST_ASSERT_EQUAL(1, maxLength);
    TEST_ASSERT_EQUAL(*data, 0xEC);
}


int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_stubborn_link_sends_data);
    UNITY_END();

    return 0;
}
