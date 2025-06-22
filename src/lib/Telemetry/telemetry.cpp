#include "telemetry.h"

#include <functional>

#if defined(TARGET_RX) || defined(UNIT_TEST)
#if defined(UNIT_TEST)
#include <iostream>
using namespace std;
#endif

#include "CRSFRouter.h"

#include "helpers.h"

// Size byte in FIFO contains bit to indicate if the frame is deleted
#define IS_DEL(size) (size & bit(7))
#define SET_DEL(size) (size | bit(7))
#define SIZE(size) (size & ~bit(7))

enum action_e
{
    ACTION_NEXT,       // continue searching queue for other messages
    ACTION_IGNORE,     // the message matches; ignore the new one
    ACTION_OVERWRITE,  // overwrite the queued message with the new one
    ACTION_APPEND      // append the message to the end of the queue
};

typedef std::function<action_e(const crsf_header_t *newMessage, const TelemetryFifo &payloads, uint16_t queuePosition)> comparator_t;

// For broadcast messages that have a 'source_id' as the first byte of the payload.
static action_e sourceId(const crsf_header_t *newMessage, const TelemetryFifo &payloads, const uint16_t queuePosition)
{
    if (payloads[queuePosition + CRSF_TELEMETRY_TYPE_INDEX + 1] == newMessage->payload[0])
    {
        return ACTION_OVERWRITE;
    }
    return ACTION_NEXT;
}

// Comparator for Ardupilot Status Text message
static action_e statusText(const crsf_header_t *newMessage, const TelemetryFifo &payloads, const uint16_t queuePosition)
{
    if (payloads[queuePosition + CRSF_TELEMETRY_TYPE_INDEX + 1] == CRSF_AP_CUSTOM_TELEM_STATUS_TEXT &&
        newMessage->payload[0] == CRSF_AP_CUSTOM_TELEM_STATUS_TEXT)
    {
        return ACTION_OVERWRITE;
    }
    return ACTION_NEXT;
}

static action_e extended_dest_origin(const crsf_header_t *newMessage, const TelemetryFifo &payloads, const uint16_t queuePosition)
{
    if (payloads[queuePosition + 3] == ((crsf_ext_header_t *)newMessage)->dest_addr &&
        payloads[queuePosition + 4] == ((crsf_ext_header_t *)newMessage)->orig_addr)
    {
        return ACTION_OVERWRITE;
    }
    return ACTION_NEXT;
}

static action_e comp_OVERWRITE(const crsf_header_t *newMessage, const TelemetryFifo &payloads, const uint16_t queuePosition)
{
    return ACTION_OVERWRITE;
}

static struct
{
    crsf_frame_type_e type;
    comparator_t comparator;
} comparators[] = {
    {CRSF_FRAMETYPE_RPM, sourceId},
    {CRSF_FRAMETYPE_TEMP, sourceId},
    {CRSF_FRAMETYPE_CELLS, sourceId},
    {CRSF_FRAMETYPE_ARDUPILOT_RESP, statusText},
    {CRSF_FRAMETYPE_DEVICE_INFO, extended_dest_origin},
    {CRSF_FRAMETYPE_PARAMETER_SETTINGS_ENTRY, comp_OVERWRITE },
};

inline bool isPrioritised(const crsf_frame_type_e frameType)
{
    return frameType >= CRSF_FRAMETYPE_DEVICE_PING && frameType <= CRSF_FRAMETYPE_PARAMETER_WRITE;
}

Telemetry::Telemetry()
{
    ResetState();
}

void Telemetry::SetCrsfBatterySensorDetected()
{
    crsfBatterySensorDetected = true;
}

void Telemetry::CheckCrsfBatterySensorDetected()
{
    if (CRSFinBuffer[CRSF_TELEMETRY_TYPE_INDEX] == CRSF_FRAMETYPE_BATTERY_SENSOR)
    {
        SetCrsfBatterySensorDetected();
    }
}

void Telemetry::SetCrsfBaroSensorDetected()
{
    crsfBaroSensorDetected = true;
}

void Telemetry::CheckCrsfBaroSensorDetected()
{
    if (CRSFinBuffer[CRSF_TELEMETRY_TYPE_INDEX] == CRSF_FRAMETYPE_BARO_ALTITUDE ||
        CRSFinBuffer[CRSF_TELEMETRY_TYPE_INDEX] == CRSF_FRAMETYPE_VARIO)
    {
        SetCrsfBaroSensorDetected();
    }
}

void Telemetry::ResetState()
{
    telemetry_state = TELEMETRY_IDLE;
    currentTelemetryByte = 0;
    prioritizedCount = 0;
    messagePayloads.flush();
}

bool Telemetry::RXhandleUARTin(CRSFConnector *origin, uint8_t data)
{
    switch(telemetry_state) {
        case TELEMETRY_IDLE:
            // Telemetry from Betaflight/iNav starts with CRSF_SYNC_BYTE (CRSF_ADDRESS_FLIGHT_CONTROLLER)
            // from a TX module it will be addressed to CRSF_ADDRESS_RADIO_TRANSMITTER (RX used as a relay)
            // and things addressed to CRSF_ADDRESS_CRSF_RECEIVER I guess we should take too since that's us, but we'll just forward them
            if (data == CRSF_SYNC_BYTE || data == CRSF_ADDRESS_RADIO_TRANSMITTER || data == CRSF_ADDRESS_CRSF_RECEIVER)
            {
                currentTelemetryByte = 0;
                telemetry_state = RECEIVING_LENGTH;
                CRSFinBuffer[0] = data;
            }
            else {
                return false;
            }

            break;
        case RECEIVING_LENGTH:
            if (data >= CRSF_MAX_PACKET_LEN)
            {
                telemetry_state = TELEMETRY_IDLE;
                return false;
            }
            else
            {
                telemetry_state = RECEIVING_DATA;
                CRSFinBuffer[CRSF_TELEMETRY_LENGTH_INDEX] = data;
            }

            break;
        case RECEIVING_DATA:
            CRSFinBuffer[currentTelemetryByte + CRSF_FRAME_NOT_COUNTED_BYTES] = data;
            currentTelemetryByte++;
            if (CRSFinBuffer[CRSF_TELEMETRY_LENGTH_INDEX] == currentTelemetryByte)
            {
                // exclude first bytes (sync byte + length), skip last byte (submitted crc)
                uint8_t crc = crsfRouter.crsf_crc.calc(CRSFinBuffer + CRSF_FRAME_NOT_COUNTED_BYTES, CRSFinBuffer[CRSF_TELEMETRY_LENGTH_INDEX] - CRSF_TELEMETRY_CRC_LENGTH);
                telemetry_state = TELEMETRY_IDLE;

                if (data == crc)
                {
                    const crsf_header_t *header = (crsf_header_t *) CRSFinBuffer;
                    crsfRouter.processMessage(origin, header);

                    // Special case to check here and not in AppendTelemetryPackage(). devAnalogVbat and vario sends
                    // direct to AppendTelemetryPackage() and we want to detect packets only received through serial.
                    if (origin != nullptr)
                    {
                        CheckCrsfBatterySensorDetected();
                        CheckCrsfBaroSensorDetected();
                    }

                    return true;
                }
                return false;
            }

            break;
    }

    return true;
}

void Telemetry::AppendTelemetryPackage(uint8_t *package)
{
    const crsf_header_t *header = (crsf_header_t *) package;
    if (header->type == CRSF_FRAMETYPE_HEARTBEAT)
    {
        return;
    }

    const uint8_t messageSize = CRSF_FRAME_SIZE(package[CRSF_TELEMETRY_LENGTH_INDEX]);
    auto action = ACTION_APPEND;
    comparator_t comparator = nullptr;

    // Find the comparator for this message type (if any)
    for (size_t i = 0 ; i < ARRAY_SIZE(comparators) ; i++)
    {
        if (comparators[i].type == header->type)
        {
            comparator = comparators[i].comparator;
            break;
        }
    }

#if defined(PLATFORM_ESP32) && SOC_CPU_CORES_NUM > 1
    std::lock_guard<std::mutex> lock(mutex);
#endif
    // If we have a comparator or this is a 'broadcast' message we will look for a matching message in the queue and default to overwrite if we find one
    uint16_t overwritePosition = 0;
    if (comparator != nullptr || header->type < CRSF_FRAMETYPE_DEVICE_PING)
    {
        for (uint16_t i = 0; i < messagePayloads.size();)
        {
            const auto size = messagePayloads[i];
            // If the message at this point in the queue is not deleted, and it matches this comparator, then we check it
            if (!IS_DEL(size) && messagePayloads[i + 1 + CRSF_TELEMETRY_TYPE_INDEX] == header->type)
            {
                const auto whatToDo = comparator == nullptr ? ACTION_OVERWRITE : comparator(header, messagePayloads, i + 1);
                if (whatToDo != ACTION_NEXT)
                {
                    overwritePosition = i;
                    action = whatToDo;
                    break;
                }
            }
            i += 1 + SIZE(size);
        }
    }
    if (isPrioritised(header->type))
    {
        prioritizedCount++;
    }

    switch (action)
    {
    case ACTION_IGNORE:
        break;
    case ACTION_OVERWRITE:
        // Check again because our initial check was performed without locking
        if (!IS_DEL(messagePayloads[overwritePosition]))
        {
            if (messagePayloads[overwritePosition] >= messageSize)
            {
                for (uint16_t i = 0 ; i<messageSize; i++)
                {
                    messagePayloads.set(overwritePosition + i + 1, package[i]);
                }
                break;
            }
            // Mark the current queued entry as deleted
            messagePayloads.set(overwritePosition, SET_DEL(messagePayloads[overwritePosition]));
        }
        // fallthrough to APPEND
    default:
        // If there's NOT enough room on the FIFO for this message, pop until there is
        while (!messagePayloads.available(messageSize + 1))
        {
            const uint8_t sz = SIZE(messagePayloads.pop());
            messagePayloads.skip(sz);
        }
        messagePayloads.push(messageSize);
        messagePayloads.pushBytes(package, messageSize);
        break;
    }
}

bool Telemetry::GetNextPayload(uint8_t* nextPayloadSize, uint8_t *currentPayload)
{
#if defined(PLATFORM_ESP32) && SOC_CPU_CORES_NUM > 1
    std::lock_guard<std::mutex> lock(mutex);
#endif
    if (prioritizedCount)
    {
        // handle prioritised messages first
        for (uint16_t i = 0; i < messagePayloads.size();)
        {
            const auto size = messagePayloads[i];
            // If the message at this point in the queue is not deleted, and it's a SETTINGS_ENTRY then we're going to return it
            if (isPrioritised((crsf_frame_type_e)messagePayloads[i + 1 + CRSF_TELEMETRY_TYPE_INDEX]))
            {
                if (!IS_DEL(size))
                {
                    prioritizedCount--;
                    // If this is the first item in the queue, use pop() instead to free the space
                    if (i == 0)
                    {
                        messagePayloads.pop();
                        messagePayloads.popBytes(currentPayload, size);
                    }
                    else // Copy the frame to the current payload
                    {
                        for (uint16_t pos = 0 ; pos < size ; pos++)
                        {
                            currentPayload[pos] = messagePayloads[i + 1 + pos];
                        }
                        // Mark the current queued entry as deleted
                        messagePayloads.set(i, SET_DEL(size));
                    }
                    // set the pointers to the payload
                    *nextPayloadSize = CRSF_FRAME_SIZE(currentPayload[CRSF_TELEMETRY_LENGTH_INDEX]);
                    return true;
                }
            }
            i += 1 + SIZE(size);
        }
        // Didn't find one, so we'll reset the counter
        prioritizedCount = 0;
    }

    // return the 'head' of the queue
    while (messagePayloads.size() > 0)
    {
        const auto size = messagePayloads.pop();
        if (IS_DEL(size))
        {
            // This message is deleted, skip it
            messagePayloads.skip(SIZE(size));
            continue;
        }
        messagePayloads.popBytes(currentPayload, size);
        *nextPayloadSize = CRSF_FRAME_SIZE(currentPayload[CRSF_TELEMETRY_LENGTH_INDEX]);
        return true;
    }

    return false;
}

// This method is only used in unit testing!
int Telemetry::UpdatedPayloadCount()
{
    int retVal = 0;
    unsigned pos = 0;
    while (pos < messagePayloads.size())
    {
        uint8_t sz = messagePayloads[pos];
        if (!IS_DEL(sz))
        {
            retVal++;
        }
        pos += 1 + SIZE(sz);
    }
    return retVal;
}

#endif
