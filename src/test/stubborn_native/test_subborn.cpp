#include <cstdint>
#include <stubborn_sender.h>
#include <stubborn_receiver.h>
#include <unity.h>
#include <iostream>
#include <bitset>

StubbornSender sender;
StubbornReceiver receiver;

void test_stubborn_link_sends_data(void)
{
    uint8_t batterySequence[] = {0xEC,10, 0x08,0,0,0,0,0,0,0,0,109};
    sender.ResetState();
    sender.SetDataToTransmit(sizeof(batterySequence), batterySequence, 1);
    uint8_t *data;
    uint8_t maxLength;
    uint8_t packageIndex;
    bool confirmValue = true;

    for(int i = 0; i < sizeof(batterySequence); i++)
    {
        sender.GetCurrentPayload(&packageIndex, &maxLength, &data);
        TEST_ASSERT_EQUAL(1, maxLength);
        TEST_ASSERT_EQUAL(i + 1, packageIndex);
        TEST_ASSERT_EQUAL(batterySequence[i], *data);
        sender.ConfirmCurrentPayload(confirmValue);
        confirmValue = !confirmValue;
    }

    sender.GetCurrentPayload(&packageIndex, &maxLength, &data);
    TEST_ASSERT_EQUAL(0, packageIndex);
    TEST_ASSERT_EQUAL(true, sender.IsActive());
    sender.ConfirmCurrentPayload(confirmValue);
    TEST_ASSERT_EQUAL(false, sender.IsActive());
}

void test_stubborn_link_sends_data_even_bytes_per_call(void)
{
    uint8_t batterySequence[] = {0xEC,10, 0x08,0,0,0,0,0,0,0,0,109};
    sender.ResetState();
    sender.SetDataToTransmit(sizeof(batterySequence), batterySequence, 2);
    uint8_t *data;
    uint8_t maxLength;
    uint8_t packageIndex;
    bool confirmValue = true;

    for(int i = 0; i < sizeof(batterySequence) / 2; i++)
    {
        sender.GetCurrentPayload(&packageIndex, &maxLength, &data);
        TEST_ASSERT_EQUAL(2, maxLength);
        TEST_ASSERT_EQUAL(batterySequence[i*2], data[0]);
        TEST_ASSERT_EQUAL(batterySequence[i*2+1], data[1]);
        sender.ConfirmCurrentPayload(confirmValue);
        confirmValue = !confirmValue;
    }
    sender.GetCurrentPayload(&packageIndex, &maxLength, &data);
    TEST_ASSERT_EQUAL(0, maxLength);
}

void test_stubborn_link_sends_data_odd_bytes_per_call(void)
{
    uint8_t batterySequence[] = {0xEC,11, 0x08,0,0,0,0,0,0,0,109};
    sender.ResetState();
    sender.SetDataToTransmit(sizeof(batterySequence), batterySequence, 3);
    uint8_t *data;
    uint8_t maxLength;
    uint8_t packageIndex;
    bool confirmValue = true;

    for(int i = 0; i < sizeof(batterySequence) / 3; i++)
    {
        sender.GetCurrentPayload(&packageIndex, &maxLength, &data);
        TEST_ASSERT_EQUAL(3, maxLength);
        TEST_ASSERT_EQUAL(batterySequence[i*3], data[0]);
        TEST_ASSERT_EQUAL(batterySequence[i*3+1], data[1]);
        TEST_ASSERT_EQUAL(batterySequence[i*3+2], data[2]);
        sender.ConfirmCurrentPayload(confirmValue);
        confirmValue = !confirmValue;
    }
    sender.GetCurrentPayload(&packageIndex, &maxLength, &data);
    TEST_ASSERT_EQUAL(2, maxLength);
    TEST_ASSERT_EQUAL(batterySequence[9], data[0]);
    TEST_ASSERT_EQUAL(batterySequence[10], data[1]);

    sender.ConfirmCurrentPayload(confirmValue);
    sender.GetCurrentPayload(&packageIndex, &maxLength, &data);
    TEST_ASSERT_EQUAL(0, maxLength);
}

void test_stubborn_link_sends_data_larger_frame_size(void)
{
    uint8_t batterySequence[] = {0xEC,10, 0x08,0,0,0,0,0,0,0,0,109};
    sender.ResetState();
    sender.SetDataToTransmit(sizeof(batterySequence), batterySequence, 13);
    uint8_t *data;
    uint8_t maxLength;
    uint8_t packageIndex;
    bool confirmValue = true;

    sender.GetCurrentPayload(&packageIndex, &maxLength, &data);
    TEST_ASSERT_EQUAL(12, maxLength);
    for(int i = 0; i < sizeof(batterySequence); i++)
    {
        TEST_ASSERT_EQUAL(batterySequence[i], data[i]);
    }

    sender.ConfirmCurrentPayload(confirmValue);
    confirmValue = !confirmValue;
    sender.GetCurrentPayload(&packageIndex, &maxLength, &data);
    TEST_ASSERT_EQUAL(0, maxLength);
}

void test_stubborn_link_receives_data(void)
{
    volatile uint8_t batterySequence[] = {0xEC,10, 0x08,0,0,0,0,0,0,0,0,109};
    uint8_t buffer[100];
    receiver.ResetState();
    receiver.SetDataToReceive(sizeof(buffer), buffer, 1);

    for(int i = 0; i < sizeof(batterySequence); i++)
    {
        bool result = receiver.ReceiveData(i+1, batterySequence + i);
        TEST_ASSERT_EQUAL(true, result);
    }
    TEST_ASSERT_EQUAL(true, receiver.ReceiveData(0, 0));

    for(int i = 0; i < sizeof(batterySequence); i++)
    {
        TEST_ASSERT_EQUAL(batterySequence[i], buffer[i]);
    }

    receiver.Unlock();
}

void test_stubborn_link_receives_data_with_multiple_bytes(void)
{
    volatile uint8_t batterySequence[] = {0xEC,10, 0x08,0,0,0,0,0,0,0,0,109,0,0,0};
    uint8_t buffer[100];
    receiver.ResetState();
    receiver.SetDataToReceive(sizeof(buffer), buffer, 5);

    for(int i = 0; i < sizeof(batterySequence) / 3; i++)
    {
        bool result = receiver.ReceiveData(i+1, batterySequence + (i * 5));
        TEST_ASSERT_EQUAL(true, result);
    }
    TEST_ASSERT_EQUAL(true, receiver.ReceiveData(0, 0));

    for(int i = 0; i < sizeof(batterySequence); i++)
    {
        TEST_ASSERT_EQUAL(batterySequence[i], buffer[i]);
    }

    receiver.Unlock();
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_stubborn_link_sends_data);
    RUN_TEST(test_stubborn_link_sends_data_even_bytes_per_call);
    RUN_TEST(test_stubborn_link_sends_data_odd_bytes_per_call);
    RUN_TEST(test_stubborn_link_sends_data_larger_frame_size);
    RUN_TEST(test_stubborn_link_receives_data);
    RUN_TEST(test_stubborn_link_receives_data_with_multiple_bytes);
    UNITY_END();

    return 0;
}
