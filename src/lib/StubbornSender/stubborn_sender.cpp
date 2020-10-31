#include <cstdint>
#include "stubborn_sender.h"

StubbornSender::StubbornSender(uint8_t maxPackageIndex)
{
    this->maxPackageIndex = maxPackageIndex;
    this->ResetState();
}

void StubbornSender::ResetState()
{
    data = 0;
    bytesPerCall = 1;
    currentOffset = 0;
    currentPackage = 0;
    length = 0;
    waitNextConfirmation = false;
    waitUntilTelemtryConfirm = true;
    waitCount = 0;
    resetState = false;
}

void StubbornSender::SetDataToTransmit(uint8_t lengthToTransmit, uint8_t* dataToTransmit, uint8_t bytesPerCall)
{
    if (lengthToTransmit / bytesPerCall >= maxPackageIndex)
    {
        return;
    }

    length = lengthToTransmit;
    data = dataToTransmit;
    currentOffset = 0;
    currentPackage = 1;
    waitCount = 0;
    waitNextConfirmation = false;
    resetState = false;
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

    if (resetState)
    {
        *packageIndex = maxPackageIndex;
        waitNextConfirmation = true;
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
    waitCount++;
    if (telemetryConfirmValue != waitUntilTelemtryConfirm)
    {
        if (waitCount > WAIT_FOR_RESYNC)
        {
            waitUntilTelemtryConfirm = !telemetryConfirmValue;
            resetState = true;
        }
        return;
    }

    waitUntilTelemtryConfirm = !waitUntilTelemtryConfirm;
    waitCount = 0;
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
