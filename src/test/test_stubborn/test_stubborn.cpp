#include <cstdint>
#include <iostream>
#include <bitset>
#include <telemetry_protocol.h>
#include <stubborn_sender.h>
#include <stubborn_receiver.h>
#include <unity.h>
#include "targets.h"
#include "helpers.h"

StubbornSender sender;
StubbornReceiver receiver;

void test_stubborn_link_sends_data(void)
{
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
    TEST_ASSERT_EQUAL(1, packageIndex);

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
    // vv note sequence is an odd number of bytes so the last packet
    // will have the opposite confirm value as a reset receiver
    uint8_t batterySequence[] = {0xEC,10,0x08,0,0,0,0,0,0,0,0,1,109};
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
    for (int i = 0; i < sizeof(batterySequence) - 1; i++)
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
    TEST_ASSERT_EQUAL(1, packageIndex);
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

/***
 * @brief: Make sure the packageIndex does not advance before the package is actually retreived with `GetCurrentPayload`
*/
void test_stubborn_link_premature_advance(void)
{
    uint8_t batterySequence[] = {0xEC,10,0x08,0,0,0,0,0,0,0,0,109};
    sender.setMaxPackageIndex(ELRS4_TELEMETRY_MAX_PACKAGES);
    sender.ResetState();
    sender.SetDataToTransmit(batterySequence, sizeof(batterySequence));
    uint8_t data[1];
    uint8_t packageIndex;
    bool confirmValue = true;

    // Simulate ACKs coming in before the packet is sent
    sender.ConfirmCurrentPayload(confirmValue);
    sender.ConfirmCurrentPayload(confirmValue);
    sender.ConfirmCurrentPayload(confirmValue);
    sender.ConfirmCurrentPayload(confirmValue);

    // Normal operation commences
    packageIndex = sender.GetCurrentPayload(data, 1);
    // packageIndex should still be the first package
    TEST_ASSERT_EQUAL(1, packageIndex);

    sender.ConfirmCurrentPayload(confirmValue);
    packageIndex = sender.GetCurrentPayload(data, 1);
    // packageIndex should now be the second package
    TEST_ASSERT_EQUAL(2, packageIndex);
}

/***
 * @brief: Make sure Receiver will fast-resync if it sees the sender has reset
*/
void test_stubborn_link_forlorn_receiver(void)
{
    uint8_t testSequence1[] = {1,2,3,4,5,6};
    uint8_t testSequence2[] = {11,12,13,14,15,16,17,18,19,20};
    sender.setMaxPackageIndex(ELRS4_TELEMETRY_MAX_PACKAGES);
    sender.ResetState();
    sender.SetDataToTransmit(testSequence1, sizeof(testSequence1));
    uint8_t packageIndex;

    uint8_t dataOta[1];
    uint8_t dataReceiver[64];
    receiver.setMaxPackageIndex(ELRS4_TELEMETRY_MAX_PACKAGES);
    receiver.ResetState();
    receiver.SetDataToReceive(dataReceiver, sizeof(dataReceiver));

    // Start by sending part of the package, but use an odd number to make
    // sure telemetry confirm is not going to match up
    for(int i = 0; i < 3; i++)
    {
        packageIndex = sender.GetCurrentPayload(dataOta, sizeof(dataOta));
        receiver.ReceiveData(packageIndex, dataOta, sizeof(dataOta));
        sender.ConfirmCurrentPayload(receiver.GetCurrentConfirm());
    }

    // Simulate the Sender rebooting and starting a different send
    // Note nothing is done to the receiver to help it figure it out
    sender.setMaxPackageIndex(ELRS4_TELEMETRY_MAX_PACKAGES);
    sender.ResetState();
    sender.SetDataToTransmit(testSequence2, sizeof(testSequence2));

    int position = 0;
    int lastPackageIndex = -1;
    while (!receiver.HasFinishedData() && position < 10000)
    {
        packageIndex = sender.GetCurrentPayload(dataOta, sizeof(dataOta));
        // If receiver is working properly, packageIndex should go 1, 2, 3, 4, 5, 6, 7, 8, 9, 0
        // If it is not working properly it will likely go 1, 2, 2, 2, 2, ... ELRS4_TELEMETRY_MAX_PACKAGES, 0, 0, 0, 0
        // Count the positions where the packageIndex has moved on
        if (lastPackageIndex != packageIndex)
        {
            ++position;
        }
        lastPackageIndex = packageIndex;

        receiver.ReceiveData(packageIndex, dataOta, sizeof(dataOta));
        sender.ConfirmCurrentPayload(receiver.GetCurrentConfirm());

    }
    TEST_ASSERT_EQUAL(sizeof(testSequence2), position);
    // The data that comes out the other side should be the second sequence, not the first
    TEST_ASSERT_EQUAL_UINT8_ARRAY(testSequence2, dataReceiver, sizeof(testSequence2));

    receiver.Unlock();
}

// Unity setup/teardown
void setUp() {}
void tearDown() {}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_stubborn_link_sends_data);
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
    RUN_TEST(test_stubborn_link_premature_advance);
    RUN_TEST(test_stubborn_link_forlorn_receiver);
    UNITY_END();

    return 0;
}
