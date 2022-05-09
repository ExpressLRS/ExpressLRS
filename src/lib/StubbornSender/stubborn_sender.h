#pragma once

#include <cstdint>

// The number of times to resend the same package index before going to RESYNC
#define SSENDER_MAX_MISSED_PACKETS 20

typedef enum {
    SENDER_IDLE = 0,
    SENDING,
    WAIT_UNTIL_NEXT_CONFIRM,
    RESYNC,
    RESYNC_THEN_SEND, // perform a RESYNC then go to SENDING
} stubborn_sender_state_e;

class StubbornSender
{
public:
    StubbornSender();
    void setMaxPackageIndex(uint8_t maxPackageIndex);
    void ResetState();
    void UpdateTelemetryRate(uint16_t airRate, uint8_t tlmRatio, uint8_t tlmBurst);
    void SetDataToTransmit(uint8_t* dataToTransmit, uint8_t lengthToTransmit);
    uint8_t GetCurrentPayload(uint8_t *outData, uint8_t maxLen);
    void ConfirmCurrentPayload(bool telemetryConfirmValue);
    bool IsActive() const { return senderState != SENDER_IDLE; }
    uint16_t GetMaxPacketsBeforeResync() const { return maxWaitCount; }
private:
    uint8_t *data;
    uint8_t length;
    uint8_t currentOffset;
    uint8_t bytesLastPayload;
    uint8_t currentPackage;
    bool waitUntilTelemetryConfirm;
    uint16_t waitCount;
    uint16_t maxWaitCount;
    uint8_t maxPackageIndex;
    volatile stubborn_sender_state_e senderState;
};
