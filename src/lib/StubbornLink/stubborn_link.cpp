#include <cstdint>
#include "stubborn_link.h"

#if defined(UNIT_TEST)
#include <iostream>
using namespace std;
#endif

void StubbornLink::ResetState()
{
    data = 0;
    bytesPerCall = 1;
    currentOffset = 0;
    currentPackage = 0;
    length = 0;
}

void StubbornLink::SetDataToTransmit(uint8_t lengthToTransmit, uint8_t* dataToTransmit)
{
    length = lengthToTransmit;
    data = dataToTransmit;
    currentOffset = 0;
    currentPackage = 1;
}

bool StubbornLink::IsActive()
{
    return length == 0;
}

void StubbornLink::GetCurrentPayload(uint8_t *packageIndex, uint8_t *count, uint8_t **currentData)
{
    *currentData = data + currentOffset;

    if (currentOffset >= length)
    {
        *count = 0;
        *currentData = 0;
        *packageIndex = 0;
        return;
    }

    *packageIndex = currentPackage;
    if (bytesPerCall > 1)
    {
        if (currentOffset + bytesPerCall <= length)
        {
            *count = bytesPerCall;
        }
        else
        {
            *count = length-currentOffset;
        }
    }
    else
    {
        *count = 1;
    }
}

void StubbornLink::ConfirmCurrentPayload()
{
    currentOffset += bytesPerCall;
    currentPackage++;
}

void StubbornLink::SetBytesPerCall(uint8_t count)
{
    bytesPerCall = count;
}

void StubbornLink::SetDataToReceive(uint8_t maxLength, uint8_t* dataToReceive)
{
    length = maxLength;
    data = dataToReceive;
    currentPackage = 1;
    currentOffset = 0;
    finishedData = false;
}

bool StubbornLink::ReceiveData(uint8_t packageIndex, uint8_t receiveData)
{
    if  (packageIndex == 0 && currentPackage > 1)
    {
        finishedData = true;
    }

    if (finishedData)
    {
        return false;
    }

    if (packageIndex == currentPackage)
    {
        data[currentOffset] = receiveData;
        currentPackage++;
        currentOffset++;
        return true;
    }

    return false;
}

uint8_t StubbornLink::GetReceivedData()
{
    if (finishedData)
    {
        uint8_t receivedLength = currentOffset;
        currentPackage = 1;
        currentOffset = 0;
        finishedData = false;
        return receivedLength;
    }
    return 0;
}
