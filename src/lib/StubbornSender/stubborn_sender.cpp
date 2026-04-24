#include <cstdint>
#include <algorithm>
#include <cstring>
#include "stubborn_sender.h"

StubbornSender::StubbornSender()
    : data(nullptr), length(0)
{
    ResetState();
}

void StubbornSender::setMaxPackageIndex(uint8_t maxPackageIndex)
{
    if (this->maxPackageIndex != maxPackageIndex)
    {
        this->maxPackageIndex = maxPackageIndex;
        ResetState();
    }
}

void StubbornSender::ResetState()
{
    bytesLastPayload = 0;
    currentOffset = 0;
    currentPackage = 1;
    waitUntilTelemetryConfirm = true;
    waitCount = 0;
    // 80 corresponds to UpdateTelemetryRate(ANY, 2, 1), which is what the TX uses in boost mode
    maxWaitCount = 80;
    senderState = SENDER_IDLE;
}

/***
 * Queues a message to send, will abort the current message if one is currently being transmitted
 ***/
void StubbornSender::SetDataToTransmit(uint8_t* dataToTransmit, uint8_t lengthToTransmit)
{
    // if (lengthToTransmit / bytesPerCall >= maxPackageIndex)
    // {
    //     return;
    // }

    length = lengthToTransmit;
    data = dataToTransmit;
    currentOffset = 0;
    currentPackage = 1;
    waitCount = 0;
    senderState = (senderState == SENDER_IDLE) ? SENDING : RESYNC_THEN_SEND;
}

/**
 * @brief: Copy up to maxLen bytes from the current package to outData
 * @returns: packageIndex
 ***/
uint8_t StubbornSender::GetCurrentPayload(uint8_t *outData, uint8_t maxLen)
{
    uint8_t packageIndex;

    bytesLastPayload = 0;
    switch (senderState)
    {
    case RESYNC:
    case RESYNC_THEN_SEND:
        packageIndex = maxPackageIndex;
        break;
    case SENDING:
        {
            bytesLastPayload = std::min((uint8_t)(length - currentOffset), maxLen);
            // If this is the last data chunk, and there has been at least one other packet
            // skip the blank packet needed for WAIT_UNTIL_NEXT_CONFIRM
            if (currentPackage > 1 && (currentOffset + bytesLastPayload) >= length)
                packageIndex = 0;
            else
                packageIndex = currentPackage;

            memcpy(outData, &data[currentOffset], bytesLastPayload);
        }
        break;
    default:
        packageIndex = 0;
    }

    return packageIndex;
}

void StubbornSender::ConfirmCurrentPayload(bool telemetryConfirmValue)
{
    stubborn_sender_state_e nextSenderState = senderState;

    switch (senderState)
    {
    case SENDING:
        if (telemetryConfirmValue != waitUntilTelemetryConfirm)
        {
            waitCount++;
            if (waitCount > maxWaitCount)
            {
                waitUntilTelemetryConfirm = !telemetryConfirmValue;
                nextSenderState = RESYNC;
            }
            break;
        }

        currentOffset += bytesLastPayload;
        if (currentOffset >= length)
        {
            // A 0th packet is always requred so the reciver can
            // differentiate a new send from a resend, if this is
            // the first packet acked, send another, else IDLE
            if (currentPackage == 1)
                nextSenderState = WAIT_UNTIL_NEXT_CONFIRM;
            else
                nextSenderState = SENDER_IDLE;
        }

        currentPackage++;
        waitUntilTelemetryConfirm = !waitUntilTelemetryConfirm;
        waitCount = 0;
        break;

    case RESYNC:
    case RESYNC_THEN_SEND:
    case WAIT_UNTIL_NEXT_CONFIRM:
        if (telemetryConfirmValue == waitUntilTelemetryConfirm)
        {
            nextSenderState = (senderState == RESYNC_THEN_SEND) ? SENDING : SENDER_IDLE;
            waitUntilTelemetryConfirm = !telemetryConfirmValue;
        }
        // switch to resync if tx does not confirm value fast enough
        else if (senderState == WAIT_UNTIL_NEXT_CONFIRM)
        {
            waitCount++;
            if (waitCount > maxWaitCount)
            {
                waitUntilTelemetryConfirm = !telemetryConfirmValue;
                nextSenderState = RESYNC;
            }
        }
        break;

    case SENDER_IDLE:
        break;
    }

    senderState = nextSenderState;
}

/*
 * Called when the telemetry ratio or air rate changes, calculate
 * the new threshold for how many times the telemetryConfirmValue
 * can be wrong in a row before giving up and going to RESYNC
 */
void StubbornSender::UpdateTelemetryRate(uint16_t airRate, uint8_t tlmRatio, uint8_t tlmBurst)
{
    // consipicuously unused airRate parameter, the wait count is strictly based on number
    // of packets, not time between the telemetry packets, or a wall clock timeout
    (void)airRate;
    // The expected number of packet periods between telemetry packets
    uint32_t packsBetween = tlmRatio * (1 + tlmBurst) / tlmBurst;
    maxWaitCount = packsBetween * SSENDER_MAX_MISSED_PACKETS;
}
