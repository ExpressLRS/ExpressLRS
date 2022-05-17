#include <cstdint>
#include <telemetry_protocol.h>
#include <stubborn_sender.h>
#include <stubborn_receiver.h>
#include <unity.h>
#include <iostream>
#include <bitset>
#include "targets.h"
#include "helpers.h"

StubbornSender sender;
StubbornReceiver receiver;

void test_stubborn_link_sends_data(void)
{
    // When the payload chunk contains non-zero data, the Sender can
    // send the last chunk in packageIndex=0 and can end early
    uint8_t batterySequence[] = {0xEC,10,0x08,0,0,0,0,0,0,0,0,109};
    sender.setMaxPackageIndex(ELRS4_TELEMETRY_MAX_PACKAGES);
    sender.ResetState();
    sender.SetDataToTransmit(batterySequence, sizeof(batterySequence));
    uint8_t data[1];
    uint8_t packageIndex;
    bool confirmValue = true;

    for(int i = 0; i < sizeof(batterySequence)-1; i++)
    {
        packageIndex = sender.GetCurrentPayload(data, 1);
        TEST_ASSERT_EQUAL(i + 1, packageIndex);
        TEST_ASSERT_EQUAL(batterySequence[i], data[0]);
        sender.ConfirmCurrentPayload(confirmValue);
        confirmValue = !confirmValue;
    }

    // Last byte in packageIndex 0
    packageIndex = sender.GetCurrentPayload(data, 1);
    TEST_ASSERT_EQUAL(0, packageIndex);
    TEST_ASSERT_EQUAL(batterySequence[sizeof(batterySequence)-1], data[0]);

    TEST_ASSERT_EQUAL(true, sender.IsActive());
    sender.ConfirmCurrentPayload(!confirmValue);
    TEST_ASSERT_EQUAL(true, sender.IsActive());
    sender.ConfirmCurrentPayload(confirmValue);
    TEST_ASSERT_EQUAL(false, sender.IsActive());
}

void test_stubborn_link_sends_data_0(void)
{
    // When the payload chunk is all zeroes, the Sender always requires
    // one extra ACK (confirm), with a dedicated packageIndex=0, payload=000000
    uint8_t batterySequence[] = {0xEC,10,0x08,0,0,0,0,0,0,0,0,0};
    sender.setMaxPackageIndex(ELRS4_TELEMETRY_MAX_PACKAGES);
    sender.ResetState();
    sender.SetDataToTransmit(batterySequence, sizeof(batterySequence));
    uint8_t data[1];
    uint8_t packageIndex;
    bool confirmValue = true;

    for(int i = 0; i < sizeof(batterySequence); i++)
    {
        packageIndex = sender.GetCurrentPayload(data, 1);
        TEST_ASSERT_EQUAL(i + 1, packageIndex);
        TEST_ASSERT_EQUAL(batterySequence[i], data[0]);
        sender.ConfirmCurrentPayload(confirmValue);
        confirmValue = !confirmValue;
    }

    packageIndex = sender.GetCurrentPayload(data, 1);
    TEST_ASSERT_EQUAL(0, packageIndex);
    TEST_ASSERT_EQUAL(true, sender.IsActive());
    sender.ConfirmCurrentPayload(!confirmValue);
    TEST_ASSERT_EQUAL(true, sender.IsActive());
    sender.ConfirmCurrentPayload(confirmValue);
    TEST_ASSERT_EQUAL(false, sender.IsActive());
}

void test_stubborn_link_sends_data_even_bytes_per_call(void)
{
    uint8_t batterySequence[] = {0xEC,10,0x08,0,0,0,0,0,0,0,0,109};
    sender.setMaxPackageIndex(ELRS4_TELEMETRY_MAX_PACKAGES);
    sender.ResetState();
    sender.SetDataToTransmit(batterySequence, sizeof(batterySequence));
    uint8_t data[2];
    uint8_t packageIndex;
    bool confirmValue = true;

    for(int i = 0; i < (sizeof(batterySequence) / 2)-1; i++)
    {
        packageIndex = sender.GetCurrentPayload(data, 2);
        TEST_ASSERT_EQUAL(i + 1, packageIndex);
        TEST_ASSERT_EQUAL(batterySequence[i*2], data[0]);
        TEST_ASSERT_EQUAL(batterySequence[i*2+1], data[1]);
        sender.ConfirmCurrentPayload(confirmValue);
        confirmValue = !confirmValue;
    }
    packageIndex = sender.GetCurrentPayload(data, 2);
    TEST_ASSERT_EQUAL(0, packageIndex);
    TEST_ASSERT_EQUAL(batterySequence[sizeof(batterySequence)-2], data[0]);
    TEST_ASSERT_EQUAL(batterySequence[sizeof(batterySequence)-1], data[1]);
}

void test_stubborn_link_sends_data_odd_bytes_per_call(void)
{
    uint8_t batterySequence[] = {0xEC,11, 0x08,0,0,0,0,0,0,0,109};
    sender.setMaxPackageIndex(ELRS4_TELEMETRY_MAX_PACKAGES);
    sender.ResetState();
    sender.SetDataToTransmit(batterySequence, sizeof(batterySequence));
    uint8_t data[3];
    uint8_t packageIndex;
    bool confirmValue = true;

    for(int i = 0; i < sizeof(batterySequence) / 3; i++)
    {
        packageIndex = sender.GetCurrentPayload(data, 3);
        TEST_ASSERT_EQUAL(batterySequence[i*3], data[0]);
        TEST_ASSERT_EQUAL(batterySequence[i*3+1], data[1]);
        TEST_ASSERT_EQUAL(batterySequence[i*3+2], data[2]);
        sender.ConfirmCurrentPayload(confirmValue);
        confirmValue = !confirmValue;
    }
    packageIndex = sender.GetCurrentPayload(data, 3);
    TEST_ASSERT_EQUAL(batterySequence[9], data[0]);
    TEST_ASSERT_EQUAL(batterySequence[10], data[1]);

    sender.ConfirmCurrentPayload(confirmValue);
    packageIndex = sender.GetCurrentPayload(data, 3);
    TEST_ASSERT_EQUAL(0, packageIndex);
}

void test_stubborn_link_sends_data_larger_frame_size(void)
{
    uint8_t batterySequence[] = {0xEC,10,0x08,0,0,0,0,0,0,0,0,109};
    sender.setMaxPackageIndex(ELRS4_TELEMETRY_MAX_PACKAGES);
    sender.ResetState();
    sender.SetDataToTransmit(batterySequence, sizeof(batterySequence));
    uint8_t data[sizeof(batterySequence)+1];
    uint8_t packageIndex;
    bool confirmValue = true;

    packageIndex = sender.GetCurrentPayload(data, sizeof(data));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(batterySequence, data, sizeof(batterySequence));
    TEST_ASSERT_EQUAL(0, packageIndex);

    sender.ConfirmCurrentPayload(confirmValue);
    packageIndex = sender.GetCurrentPayload(data, sizeof(data));
    TEST_ASSERT_EQUAL(0, packageIndex);
}

void test_stubborn_link_receives_data(void)
{
    uint8_t batterySequence[] = {0xEC,10,0x08,0,0,0,0,0,0,0,0,109};
    uint8_t data[100];
    receiver.setMaxPackageIndex(ELRS4_TELEMETRY_MAX_PACKAGES);
    receiver.ResetState();
    receiver.SetDataToReceive(data, sizeof(data));

    for(int i = 1; i <= sizeof(batterySequence); i++)
    {
        uint8_t idx = (i == sizeof(batterySequence)) ? 0 : i;
        receiver.ReceiveData(idx, &batterySequence[i-1], 1);
    }

    TEST_ASSERT_EQUAL_UINT8_ARRAY(batterySequence, data, sizeof(batterySequence));

    receiver.Unlock();
}

void test_stubborn_link_receives_data_with_multiple_bytes(void)
{
    uint8_t batterySequence[] = {0xEC,10,0x08,0,0,0,0,0,0,0,0,109,0,0,0};
    uint8_t data[100];
    receiver.setMaxPackageIndex(ELRS4_TELEMETRY_MAX_PACKAGES);
    receiver.ResetState();
    receiver.SetDataToReceive(data, sizeof(data));

    for(int i = 0; i < sizeof(batterySequence) / 3; i++)
    {
        uint8_t idx = (i >= sizeof(batterySequence)-6) ? 0 : i + 1;
        receiver.ReceiveData(idx, &batterySequence[i * 5], 5);
    }

    TEST_ASSERT_EQUAL_UINT8_ARRAY(batterySequence, data, sizeof(batterySequence));

    receiver.Unlock();
}

void test_stubborn_link_resyncs(void)
{
    uint8_t batterySequence[] = {0xEC,10,0x08,0,0,0,0,0,0,0,0,109};
    uint8_t buffer[100];
    uint8_t data[1];
    uint8_t packageIndex;

    receiver.setMaxPackageIndex(ELRS4_TELEMETRY_MAX_PACKAGES);
    receiver.ResetState();
    receiver.SetDataToReceive(buffer, sizeof(buffer));

    sender.setMaxPackageIndex(ELRS4_TELEMETRY_MAX_PACKAGES);
    sender.ResetState();
    sender.SetDataToTransmit(batterySequence, sizeof(batterySequence));

    // send and confirm two packages
    packageIndex = sender.GetCurrentPayload(data, 1);
    receiver.ReceiveData(packageIndex, data, 1);
    sender.ConfirmCurrentPayload(receiver.GetCurrentConfirm());

    packageIndex = sender.GetCurrentPayload(data, 1);
    receiver.ReceiveData(packageIndex, data, 1);
    sender.ConfirmCurrentPayload(receiver.GetCurrentConfirm());

    // now reset receiver so the receiver sends the wrong confirm value
    // all communication would be stuck
    receiver.ResetState();
    receiver.SetDataToReceive(buffer, sizeof(buffer));

    // wait for resync to happen
    for(int i = 0; i < sender.GetMaxPacketsBeforeResync() + 1; i++)
    {
        packageIndex = sender.GetCurrentPayload(data, 1);
        TEST_ASSERT_EQUAL(3, packageIndex);
        receiver.ReceiveData(packageIndex, data, 1);
        sender.ConfirmCurrentPayload(receiver.GetCurrentConfirm());
    }

    // resync active
    packageIndex = sender.GetCurrentPayload(data, 1);
    TEST_ASSERT_EQUAL(ELRS4_TELEMETRY_MAX_PACKAGES, packageIndex);
    receiver.ReceiveData(packageIndex, data, 1);
    sender.ConfirmCurrentPayload(receiver.GetCurrentConfirm());
    TEST_ASSERT_EQUAL(false, sender.IsActive());

    // both are in sync again
    sender.SetDataToTransmit(batterySequence, sizeof(batterySequence));
    packageIndex = sender.GetCurrentPayload(data, 1);
    receiver.ReceiveData(packageIndex, data, 1);
    sender.ConfirmCurrentPayload(receiver.GetCurrentConfirm());

    packageIndex = sender.GetCurrentPayload(data, 1);
    TEST_ASSERT_EQUAL(2, packageIndex);

    receiver.Unlock();
}

void test_stubborn_link_resyncs_during_last_confirm(void)
{
    // 0 last byte so the separate confirm is used           vvv
    uint8_t batterySequence[] = {0xEC,10,0x08,0,0,0,0,0,0,0,0,0};
    uint8_t buffer[100];
    uint8_t data[1];
    uint8_t packageIndex;

    receiver.setMaxPackageIndex(ELRS4_TELEMETRY_MAX_PACKAGES);
    receiver.ResetState();
    receiver.SetDataToReceive(buffer, sizeof(buffer));

    sender.setMaxPackageIndex(ELRS4_TELEMETRY_MAX_PACKAGES);
    sender.ResetState();
    sender.SetDataToTransmit(batterySequence, sizeof(batterySequence));

    // send and confirm twelve packages
    for (int i = 0; i < sizeof(batterySequence); i++)
    {
        packageIndex = sender.GetCurrentPayload(data, 1);
        receiver.ReceiveData(packageIndex, data, 1);
        sender.ConfirmCurrentPayload(receiver.GetCurrentConfirm());
    }

    packageIndex = sender.GetCurrentPayload(data, 1);
    TEST_ASSERT_EQUAL(0, packageIndex);

    // now reset receiver so the receiver sends the wrong confirm value
    // all communication would be stuck since sender waits for final confirmation
    receiver.ResetState();
    receiver.SetDataToReceive(buffer, sizeof(buffer));

    // wait for resync to happen
    for(int i = 0; i < sender.GetMaxPacketsBeforeResync() + 1; i++)
    {
        packageIndex = sender.GetCurrentPayload(data, 1);
        TEST_ASSERT_EQUAL(0, packageIndex);
        receiver.ReceiveData(packageIndex, data, 1);
        sender.ConfirmCurrentPayload(receiver.GetCurrentConfirm());
    }

    // resync active
    packageIndex = sender.GetCurrentPayload(data, 1);
    TEST_ASSERT_EQUAL(ELRS4_TELEMETRY_MAX_PACKAGES, packageIndex);
    receiver.ReceiveData(packageIndex, data, 1);
    sender.ConfirmCurrentPayload(receiver.GetCurrentConfirm());
    TEST_ASSERT_EQUAL(false, sender.IsActive());

    // both are in sync again
    sender.SetDataToTransmit(batterySequence, sizeof(batterySequence));
    packageIndex = sender.GetCurrentPayload(data, 1);
    receiver.ReceiveData(packageIndex, data, 1);
    sender.ConfirmCurrentPayload(receiver.GetCurrentConfirm());

    packageIndex = sender.GetCurrentPayload(data, 1);
    TEST_ASSERT_EQUAL(2, packageIndex);

    receiver.Unlock();
}

void test_stubborn_link_sends_data_until_confirmation(void)
{
    uint8_t batterySequence[] = {0xEC,10,0x08,0,0,0,0,0,0,0,0,109};
    uint8_t data[1];
    uint8_t packageIndex;
    uint8_t buffer[100];

    sender.setMaxPackageIndex(ELRS4_TELEMETRY_MAX_PACKAGES);
    sender.ResetState();
    sender.SetDataToTransmit(batterySequence, sizeof(batterySequence));

    receiver.setMaxPackageIndex(ELRS4_TELEMETRY_MAX_PACKAGES);
    receiver.ResetState();
    receiver.SetDataToReceive(buffer, sizeof(buffer));

    for(int i = 0; i < sender.GetMaxPacketsBeforeResync(); i++)
    {
        packageIndex = sender.GetCurrentPayload(data, 1);
        TEST_ASSERT_EQUAL(1, packageIndex);
        receiver.ReceiveData(packageIndex, data, 1);
        sender.ConfirmCurrentPayload(!receiver.GetCurrentConfirm());
    }

    sender.ConfirmCurrentPayload(receiver.GetCurrentConfirm());
    packageIndex = sender.GetCurrentPayload(data, 1);
    TEST_ASSERT_EQUAL(2, packageIndex);
}

void test_stubborn_link_multiple_packages(void)
{
    uint8_t batterySequence[] = {0xEC,10,0x08,0,0,0,0,0,0,0,0,109};
    uint8_t data[1];
    uint8_t packageIndex;
    uint8_t buffer[100];

    receiver.setMaxPackageIndex(ELRS4_TELEMETRY_MAX_PACKAGES);
    receiver.ResetState();
    receiver.SetDataToReceive(buffer, sizeof(buffer));

    sender.setMaxPackageIndex(ELRS4_TELEMETRY_MAX_PACKAGES);
    sender.ResetState();

    for (int i = 0; i < 3; i++)
    {
        sender.SetDataToTransmit(batterySequence, sizeof(batterySequence));
        TEST_ASSERT_EQUAL(true, sender.IsActive());
        for(int currentByte = 0; currentByte < sizeof(batterySequence); currentByte++)
        {
            packageIndex = sender.GetCurrentPayload(data, 1);
            receiver.ReceiveData(packageIndex, data, 1);
            sender.ConfirmCurrentPayload(receiver.GetCurrentConfirm());
        }

        TEST_ASSERT_EQUAL(false, sender.IsActive());
        TEST_ASSERT_EQUAL(true, receiver.HasFinishedData());
        receiver.Unlock();
        TEST_ASSERT_EQUAL(false, receiver.HasFinishedData());
    }

}

static void test_stubborn_link_resync_then_send(void)
{
    uint8_t testSequence1[] = {1,2,3,4,5,6,7,8,9,10};
    uint8_t testSequence2[] = {11,12,13,14,15,16,17,18,19,20};
    uint8_t buffer[100];
    uint8_t data[1];
    uint8_t maxLength;
    uint8_t packageIndex;

    receiver.setMaxPackageIndex(ELRS4_TELEMETRY_MAX_PACKAGES);
    receiver.ResetState();
    receiver.SetDataToReceive(buffer, sizeof(buffer));

    sender.setMaxPackageIndex(ELRS4_TELEMETRY_MAX_PACKAGES);
    sender.ResetState();
    sender.SetDataToTransmit(testSequence1, sizeof(testSequence1));

    // send and confirm two packages
    packageIndex = sender.GetCurrentPayload(data, 1);
    receiver.ReceiveData(packageIndex, data, 1);
    sender.ConfirmCurrentPayload(receiver.GetCurrentConfirm());

    packageIndex = sender.GetCurrentPayload(data, 1);
    receiver.ReceiveData(packageIndex, data, 1);
    sender.ConfirmCurrentPayload(receiver.GetCurrentConfirm());

    // Abort the transfer by changing the payload
    sender.SetDataToTransmit(testSequence2, sizeof(testSequence2));

    // Send next packet, which should be a RESYNC
    packageIndex = sender.GetCurrentPayload(data, 1);
    TEST_ASSERT_EQUAL(ELRS4_TELEMETRY_MAX_PACKAGES, packageIndex);
    receiver.ReceiveData(packageIndex, data, 1);
    sender.ConfirmCurrentPayload(receiver.GetCurrentConfirm());

    // Complete the transfer
    int maxSends = ELRS4_TELEMETRY_MAX_PACKAGES + 1;
    while (!receiver.HasFinishedData() && maxSends)
    {
        packageIndex = sender.GetCurrentPayload(data, 1);
        receiver.ReceiveData(packageIndex, data, 1);
        sender.ConfirmCurrentPayload(receiver.GetCurrentConfirm());
    }

    // Should not have exhausted all the sends to get the package to go
    TEST_ASSERT_NOT_EQUAL(0, maxSends);
    // Make sure the second package was received, not the first
    TEST_ASSERT_EQUAL_UINT8_ARRAY(testSequence2, buffer, ARRAY_SIZE(testSequence2));
}

void test_stubborn_link_variable_size_per_call(void)
{
    uint8_t batterySequence[] = {0xEC,10,0x08,0,0,0,0,0,0,0,0,109};
    sender.setMaxPackageIndex(ELRS4_TELEMETRY_MAX_PACKAGES);
    sender.ResetState();
    sender.SetDataToTransmit(batterySequence, sizeof(batterySequence));
    uint8_t data[2];
    uint8_t packageIndex;
    bool confirmValue = true;

    uint8_t expectedPos = 0;
    uint8_t expectedPackageIndex = 0;
    bool doMultibyte = true;
    while (sender.IsActive())
    {
        uint8_t bytesPerCall = doMultibyte ? 2 : 1;
        packageIndex = sender.GetCurrentPayload(data, bytesPerCall);

        if (packageIndex != 0)
        {
            expectedPackageIndex++;
            TEST_ASSERT_EQUAL(expectedPackageIndex, packageIndex);

            for (unsigned i=0; i<bytesPerCall && expectedPos<sizeof(batterySequence); ++i)
                TEST_ASSERT_EQUAL(batterySequence[expectedPos++], data[i]);

        }

        sender.ConfirmCurrentPayload(confirmValue);
        confirmValue = !confirmValue;
        doMultibyte = !doMultibyte;
    }
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_stubborn_link_sends_data);
    RUN_TEST(test_stubborn_link_sends_data_0);
    RUN_TEST(test_stubborn_link_sends_data_even_bytes_per_call);
    RUN_TEST(test_stubborn_link_sends_data_odd_bytes_per_call);
    RUN_TEST(test_stubborn_link_sends_data_larger_frame_size);
    RUN_TEST(test_stubborn_link_receives_data);
    RUN_TEST(test_stubborn_link_receives_data_with_multiple_bytes);
    RUN_TEST(test_stubborn_link_resyncs);
    RUN_TEST(test_stubborn_link_resyncs_during_last_confirm);
    RUN_TEST(test_stubborn_link_multiple_packages);
    RUN_TEST(test_stubborn_link_resync_then_send);
    RUN_TEST(test_stubborn_link_variable_size_per_call);
    UNITY_END();

    return 0;
}
