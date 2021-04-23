#pragma once

#include <cstdint>

// The number of times to resend the same package index before going to RESYNC
#define SSENDER_MAX_MISSED_PACKETS 20

typedef enum {
    SENDER_IDLE = 0,
    SENDING,
    SEND_NEXT,
    WAIT_UNTIL_NEXT_CONFIRM,
    RESYNC
} stubborn_sender_state_s;

class StubbornSender
{
public:
    StubbornSender(uint8_t maxPackageIndex);
    void ResetState();
    void UpdateTelemetryRate(uint16_t airRate, uint8_t tlmRatio, uint8_t tlmBurst);
    void SetDataToTransmit(uint8_t lengthToTransmit, uint8_t* dataToTransmit, uint8_t bytesPerCall);
    void GetCurrentPayload(uint8_t *packageIndex, uint8_t *count, uint8_t **currentData);
    void ConfirmCurrentPayload(bool telemetryConfirmValue);
    bool IsActive();
private:
    uint8_t *data;
    uint8_t length;
    uint8_t bytesPerCall;
    uint8_t currentOffset;
    uint8_t currentPackage;
    bool waitUntilTelemetryConfirm;
    bool resetState;
    uint16_t waitCount;
    uint16_t maxWaitCount;
    uint8_t maxPackageIndex;
    stubborn_sender_state_s senderState;
};
