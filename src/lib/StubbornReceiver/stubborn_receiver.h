#pragma once

#include <cstdint>

class StubbornReceiver
{
public:
    StubbornReceiver(uint8_t maxPackageIndex);
    void ResetState();
    void SetDataToReceive(uint8_t maxLength, uint8_t* dataToReceive, uint8_t bytesPerCall);
    void ReceiveData(uint8_t const packageIndex, uint8_t const * const receiveData);
    bool HasFinishedData();
    void Unlock();
    bool GetCurrentConfirm();
private:
    uint8_t *data;
    volatile bool finishedData;
    volatile uint8_t length;
    volatile uint8_t bytesPerCall;
    volatile uint8_t currentOffset;
    volatile uint8_t currentPackage;
    volatile bool telemetryConfirm;
    volatile uint8_t maxPackageIndex;
};
