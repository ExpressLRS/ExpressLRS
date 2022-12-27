#pragma once

#include <cstdint>

class StubbornReceiver
{
public:
    StubbornReceiver();
    void setMaxPackageIndex(uint8_t maxPackageIndex);
    void ResetState();
    void SetDataToReceive(uint8_t* dataToReceive, uint8_t maxLength);
    void ReceiveData(uint8_t const packageIndex, uint8_t const * const receiveData, uint8_t dataLen);
    bool HasFinishedData();
    void Unlock();
    bool GetCurrentConfirm();
private:
    uint8_t *data;
    bool finishedData;
    uint8_t length;
    uint8_t currentOffset;
    uint8_t currentPackage;
    bool telemetryConfirm;
    uint8_t maxPackageIndex;
};
