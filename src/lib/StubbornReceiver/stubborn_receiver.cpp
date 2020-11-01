#include <cstdint>
#include "stubborn_receiver.h"

StubbornReceiver::StubbornReceiver(uint8_t maxPackageIndex)
{
    this->maxPackageIndex = maxPackageIndex;
    this->ResetState();
}

void StubbornReceiver::ResetState()
{
    data = 0;
    bytesPerCall = 1;
    currentOffset = 0;
    currentPackage = 0;
    length = 0;
    telemetryConfirm = false;
}

bool StubbornReceiver::GetCurrentConfirm()
{
    return telemetryConfirm;
}

void StubbornReceiver::SetDataToReceive(uint8_t maxLength, uint8_t* dataToReceive, uint8_t bytesPerCall)
{
    length = maxLength;
    data = dataToReceive;
    currentPackage = 1;
    currentOffset = 0;
    finishedData = false;
    this->bytesPerCall = bytesPerCall;
}

void StubbornReceiver::ReceiveData(uint8_t packageIndex, volatile uint8_t* receiveData)
{
    if  (packageIndex == 0 && currentPackage > 1)
    {
        finishedData = true;
        telemetryConfirm = !telemetryConfirm;
        return;
    }

    if (packageIndex == maxPackageIndex)
    {
        telemetryConfirm = !telemetryConfirm;
        currentPackage = 1;
        currentOffset = 0;
        finishedData = false;
        return;
    }

    if (finishedData)
    {
        return;
    }

    if (packageIndex == currentPackage)
    {
        for (uint8_t i = 0; i < bytesPerCall; i++)
        {
            data[currentOffset++] = *(receiveData + i);
        }

        currentPackage++;
        telemetryConfirm = !telemetryConfirm;
        return;
    }

    return;
}

bool StubbornReceiver::HasFinishedData()
{
    return finishedData;
}

void StubbornReceiver::Unlock()
{
    if (finishedData)
    {
        currentPackage = 1;
        currentOffset = 0;
        finishedData = false;
    }
}
