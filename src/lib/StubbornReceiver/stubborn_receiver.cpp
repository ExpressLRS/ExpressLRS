#include <cstdint>
#include <algorithm>
#include "stubborn_receiver.h"

StubbornReceiver::StubbornReceiver()
{
    ResetState();
    data = nullptr;
    length = 0;
}

void StubbornReceiver::setMaxPackageIndex(uint8_t maxPackageIndex)
{
    if (this->maxPackageIndex != maxPackageIndex)
    {
        this->maxPackageIndex = maxPackageIndex;
        ResetState();
    }
}

void StubbornReceiver::ResetState()
{
    currentOffset = 0;
    currentPackage = 0;
    telemetryConfirm = false;
}

bool StubbornReceiver::GetCurrentConfirm()
{
    return telemetryConfirm;
}

void StubbornReceiver::SetDataToReceive(uint8_t* dataToReceive, uint8_t maxLength)
{
    length = maxLength;
    data = dataToReceive;
    currentPackage = 1;
    currentOffset = 0;
    finishedData = false;
}

void StubbornReceiver::ReceiveData(uint8_t const packageIndex, uint8_t const * const receiveData, uint8_t dataLen)
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
        uint8_t len = std::min((uint8_t)(length - currentOffset), dataLen);
        for (uint8_t i = 0; i < len; i++)
        {
            data[currentOffset++] = receiveData[i];
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
