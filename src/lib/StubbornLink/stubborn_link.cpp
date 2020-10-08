#include <cstdint>
#include "stubborn_link.h"

uint8_t* StubbornLink::data = 0;
uint8_t StubbornLink::length = 0;
uint8_t StubbornLink::bytesPerCall = 1;
uint8_t StubbornLink::currentOffset = 0;

void StubbornLink::ResetState()
{
    data = 0;
    bytesPerCall = 1;
    currentOffset = 0;
}

void StubbornLink::SetDataToTransmit(uint8_t lengthToTransmit, uint8_t* dataToTransmit)
{
    length = lengthToTransmit;
    data = dataToTransmit;
    currentOffset = 0;
}

void StubbornLink::GetCurrentPayload(uint8_t *count, uint8_t **currentData)
{
    *currentData = data + currentOffset;
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
    if (currentOffset + bytesPerCall < length)
    {
        currentOffset += bytesPerCall;
    }
}

void StubbornLink::SetBytesPerCall(uint8_t count)
{
    bytesPerCall = count;
}
