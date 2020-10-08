#pragma once

#include <cstdint>

class StubbornLink
{
public:
    void ResetState();
    void SetDataToTransmit(uint8_t lengthToTransmit, uint8_t* dataToTransmit);
    void GetCurrentPayload(uint8_t *count, uint8_t **currentData);
    void ConfirmCurrentPayload();
    void SetBytesPerCall(uint8_t count);

private:
    static uint8_t *data;
    static uint8_t length;
    static uint8_t bytesPerCall;
    static uint8_t currentOffset;
};
