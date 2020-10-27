#include <cstdint>
#include "stubborn_sender.h"

void StubbornSender::ResetState()
{
    data = 0;
    bytesPerCall = 1;
    currentOffset = 0;
    currentPackage = 0;
    length = 0;
    waitNextConfirmation = false;
    waitUntilTelemtryConfirm = true;
}

void StubbornSender::SetDataToTransmit(uint8_t lengthToTransmit, uint8_t* dataToTransmit, uint8_t bytesPerCall)
{
    length = lengthToTransmit;
    data = dataToTransmit;
    currentOffset = 0;
    currentPackage = 1;
    waitNextConfirmation = false;
    this->bytesPerCall = bytesPerCall;
}

bool StubbornSender::IsActive()
{
    return length != 0;
}

void StubbornSender::GetCurrentPayload(uint8_t *packageIndex, uint8_t *count, uint8_t **currentData)
{
    *count = 0;
    *currentData = 0;
    *packageIndex = 0;
    if (!IsActive())
    {
        return;
    }

    if (currentOffset >= length)
    {
        waitNextConfirmation = true;
        return;
    }

    *currentData = data + currentOffset;

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

void StubbornSender::ConfirmCurrentPayload(bool telemetryConfirmValue)
{
    if (telemetryConfirmValue != waitUntilTelemtryConfirm)
    {
        return;
    }
    waitUntilTelemtryConfirm = !waitUntilTelemtryConfirm;
    if (waitNextConfirmation)
    {
        length = 0;
    }
    else
    {
        currentOffset += bytesPerCall;
        currentPackage++;
    }
}
