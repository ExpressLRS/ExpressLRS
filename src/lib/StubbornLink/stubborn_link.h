#pragma once

#include <cstdint>

class StubbornLink
{
public:
    void ResetState();
    void SetDataToTransmit(uint8_t lengthToTransmit, uint8_t* dataToTransmit);
    void GetCurrentPayload(uint8_t *packageIndex, uint8_t *count, uint8_t **currentData);
    bool IsActive();
    void ConfirmCurrentPayload();
    void SetBytesPerCall(uint8_t count);

    void SetDataToReceive(uint8_t maxLength, uint8_t* dataToReceive);
    bool ReceiveData(uint8_t packageIndex, uint8_t data);
    uint8_t GetReceivedData();
private:
    uint8_t *data;
    bool finishedData;
    volatile uint8_t length;
    volatile uint8_t bytesPerCall;
    volatile uint8_t currentOffset;
    volatile uint8_t currentPackage;
};
