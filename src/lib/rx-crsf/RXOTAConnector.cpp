#include "RXOTAConnector.h"

#include "helpers.h"

#include <functional>
#include <unordered_map>

// Size byte in FIFO contains bit to indicate if the frame is deleted
#define IS_DEL(size) (size & bit(7))
#define SET_DEL(size) (size | bit(7))
#define SIZE(size) (size & ~bit(7))

enum action_e
{
    ACTION_NEXT,      // continue searching queue for other messages
    ACTION_IGNORE,    // the message matches; ignore the new one
    ACTION_OVERWRITE, // overwrite the queued message with the new one
    ACTION_APPEND     // append the message to the end of the queue
};

typedef std::function<action_e(const crsf_header_t *newMessage, TelemetryFifo &payloads, uint16_t queuePosition)> comparator_t;

// For broadcast messages that have a 'source_id' as the first byte of the payload.
static action_e sourceId(const crsf_header_t *newMessage, const TelemetryFifo &payloads, const uint16_t queuePosition)
{
    if (payloads[queuePosition + CRSF_TELEMETRY_TYPE_INDEX + 1] == ((uint8_t *)newMessage)[CRSF_TELEMETRY_TYPE_INDEX + 1])
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

// Comparator for extended messages with the same destination and origin address.
static action_e extendedSameDestOrigin(const crsf_header_t *newMessage, const TelemetryFifo &payloads, const uint16_t queuePosition)
{
    if (payloads[queuePosition + 3] == ((crsf_ext_header_t *)newMessage)->dest_addr && payloads[queuePosition + 4] == ((crsf_ext_header_t *)newMessage)->orig_addr)
    {
        return ACTION_OVERWRITE;
    }
    return ACTION_NEXT;
}

static std::unordered_map<crsf_frame_type_e, comparator_t> comparators = {
    {CRSF_FRAMETYPE_RPM, sourceId},
    {CRSF_FRAMETYPE_TEMP, sourceId},
    {CRSF_FRAMETYPE_CELLS, sourceId},
    {CRSF_FRAMETYPE_ARDUPILOT_RESP, statusText},
    {CRSF_FRAMETYPE_DEVICE_INFO, extendedSameDestOrigin},
    {CRSF_FRAMETYPE_PARAMETER_SETTINGS_ENTRY, extendedSameDestOrigin },
};

inline bool isPrioritised(const crsf_frame_type_e frameType)
{
    return frameType >= CRSF_FRAMETYPE_DEVICE_PING && frameType <= CRSF_FRAMETYPE_PARAMETER_WRITE;
}

RXOTAConnector::RXOTAConnector()
{
    addDevice(CRSF_ADDRESS_CRSF_TRANSMITTER);
}

bool RXOTAConnector::GetNextPayload(uint8_t *nextPayloadSize, uint8_t *payloadData)
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
                        messagePayloads.popBytes(payloadData, size);
                    }
                    else // Copy the frame to the current payload
                    {
                        for (uint16_t pos = 0 ; pos < size ; pos++)
                        {
                            payloadData[pos] = messagePayloads[i + 1 + pos];
                        }
                        // Mark the current queued entry as deleted
                        messagePayloads.set(i, SET_DEL(size));
                    }
                    // set the pointers to the payload
                    *nextPayloadSize = CRSF_FRAME_SIZE(payloadData[CRSF_TELEMETRY_LENGTH_INDEX]);
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
        messagePayloads.popBytes(payloadData, size);
        *nextPayloadSize = CRSF_FRAME_SIZE(payloadData[CRSF_TELEMETRY_LENGTH_INDEX]);
        return true;
    }

    *nextPayloadSize = 0;
    return false;
}

void RXOTAConnector::forwardMessage(const crsf_header_t *message)
{
    const uint8_t messageSize = CRSF_FRAME_SIZE(((uint8_t *)message)[CRSF_TELEMETRY_LENGTH_INDEX]);

    auto action = ACTION_APPEND;
    uint16_t overwritePosition = 0;
    const auto comparator = comparators.find(message->type);

#if defined(PLATFORM_ESP32) && SOC_CPU_CORES_NUM > 1
    std::lock_guard<std::mutex> lock(mutex);
#endif

    // If we have a comparator or this is a 'broadcast' message we will look for a matching message in the queue and default to overwrite if we find one
    if (comparator != comparators.end() || message->type < CRSF_FRAMETYPE_DEVICE_PING)
    {
        for (uint16_t i = 0; i < messagePayloads.size();)
        {
            const auto size = messagePayloads[i];
            // If the message at this point in the queue is not deleted, and it matches this comparator, then we check it
            if (!IS_DEL(size) && messagePayloads[i + 1 + CRSF_TELEMETRY_TYPE_INDEX] == message->type)
            {
                const auto whatToDo = comparator == comparators.end() ? ACTION_OVERWRITE : comparator->second(message, messagePayloads, i + 1);
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
    if (isPrioritised(message->type))
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
                for (uint16_t i = 0; i < messageSize; i++)
                {
                    messagePayloads.set(overwritePosition + i + 1, ((uint8_t *)message)[i]);
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
        messagePayloads.pushBytes((uint8_t *)message, messageSize);
    }
}
