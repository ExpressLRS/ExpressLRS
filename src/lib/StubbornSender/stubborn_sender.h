#pragma once

#include <cstdint>

class StubbornSender
{
public:
    void ResetState();
    void SetDataToTransmit(uint8_t lengthToTransmit, uint8_t* dataToTransmit, uint8_t bytesPerCall);
    void GetCurrentPayload(uint8_t *packageIndex, uint8_t *count, uint8_t **currentData);
    void ConfirmCurrentPayload(bool telemetryConfirmValue);
    bool IsActive();
private:
    uint8_t *data;
    volatile uint8_t length;
    volatile uint8_t bytesPerCall;
    volatile uint8_t currentOffset;
    volatile uint8_t currentPackage;
    volatile bool waitNextConfirmation;
    volatile bool waitUntilTelemtryConfirm;
};
