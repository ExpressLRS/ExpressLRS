#include "telemetry.h"
#include "logging.h"

#include <functional>
#include <map>

#if CRSF_RX_MODULE

#if defined(USE_MSP_WIFI) // enable MSP2WIFI for RX only at the moment
#include "tcpsocket.h"
extern TCPSOCKET wifi2tcp;
#endif

#if defined(HAS_MSP_VTX)
#include "devMSPVTX.h"
#endif

#include "crsf2msp.h"

enum action_e
{
    ACTION_NEXT,       // skip to the next message in the queue
    ACTION_IGNORE,     // the message matches; ignore the new one
    ACTION_OVERWRITE,  // overwrite the queued message with the new one
    ACTION_APPEND      // append the message to the end of the queue
};

typedef std::function<action_e(const crsf_header_t *newMessage, FIFO<2048> &payloads, uint16_t queuePosition)> comparator_t;

// For broadcast messages that have a 'source_id' as the first byte of the payload.
static action_e sourceId(const crsf_header_t *newMessage, const FIFO<2048> &payloads, const uint16_t queuePosition)
{
    if (payloads[queuePosition + CRSF_TELEMETRY_TYPE_INDEX + 1] == ((uint8_t*)newMessage)[CRSF_TELEMETRY_TYPE_INDEX + 1])
    {
        return ACTION_OVERWRITE;
    }
    return ACTION_NEXT;
}

// Comparator for Ardupilot Status Text message
static action_e statusText(const crsf_header_t *newMessage, const FIFO<2048> &payloads, const uint16_t queuePosition)
{
    if (payloads[queuePosition + CRSF_TELEMETRY_TYPE_INDEX + 1] == CRSF_AP_CUSTOM_TELEM_STATUS_TEXT &&
    ((uint8_t*)newMessage)[CRSF_TELEMETRY_TYPE_INDEX + 1] == CRSF_AP_CUSTOM_TELEM_STATUS_TEXT)
    {
        return ACTION_OVERWRITE;
    }
    return ACTION_NEXT;
}

static std::map<crsf_frame_type_e, comparator_t> comparators = {
    {CRSF_FRAMETYPE_GPS, nullptr},
    {CRSF_FRAMETYPE_VARIO, nullptr},
    {CRSF_FRAMETYPE_BATTERY_SENSOR, nullptr},
    {CRSF_FRAMETYPE_BARO_ALTITUDE, nullptr},
    {CRSF_FRAMETYPE_AIRSPEED, nullptr},
    {CRSF_FRAMETYPE_RPM, sourceId},
    {CRSF_FRAMETYPE_TEMP, sourceId},
    {CRSF_FRAMETYPE_CELLS, sourceId},
    {CRSF_FRAMETYPE_ATTITUDE, nullptr},
    {CRSF_FRAMETYPE_FLIGHT_MODE, nullptr},
    {CRSF_FRAMETYPE_ARDUPILOT_RESP, statusText},
};

static int settingsCount = 0;
inline bool isPrioritised(const crsf_frame_type_e frameType)
{
    return frameType >= CRSF_FRAMETYPE_DEVICE_PING && frameType <= CRSF_FRAMETYPE_PARAMETER_WRITE;
}

Telemetry::Telemetry()
{
    ResetState();
}

bool Telemetry::ShouldCallBootloader()
{
    bool bootloader = callBootloader;
    callBootloader = false;
    return bootloader;
}

bool Telemetry::ShouldCallEnterBind()
{
    bool enterBind = callEnterBind;
    callEnterBind = false;
    return enterBind;
}

bool Telemetry::ShouldCallUpdateModelMatch()
{
    bool updateModelMatch = callUpdateModelMatch;
    callUpdateModelMatch = false;
    return updateModelMatch;
}

bool Telemetry::ShouldSendDeviceFrame()
{
    bool deviceFrame = sendDeviceFrame;
    sendDeviceFrame = false;
    return deviceFrame;
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
    currentPayloadIndex = 0;
    twoslotLastQueueIndex = 0;
    receivedPackages = 0;
    messagePayloads.flush();
}

bool Telemetry::RXhandleUARTin(uint8_t data)
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
                uint8_t crc = crsf_crc.calc(CRSFinBuffer + CRSF_FRAME_NOT_COUNTED_BYTES, CRSFinBuffer[CRSF_TELEMETRY_LENGTH_INDEX] - CRSF_TELEMETRY_CRC_LENGTH);
                telemetry_state = TELEMETRY_IDLE;

                if (data == crc)
                {
                    AppendTelemetryPackage(CRSFinBuffer);

                    // Special case to check here and not in AppendTelemetryPackage(). devAnalogVbat and vario sends
                    // direct to AppendTelemetryPackage() and we want to detect packets only received through serial.
                    CheckCrsfBatterySensorDetected();
                    CheckCrsfBaroSensorDetected();

                    receivedPackages++;
                    return true;
                }
                return false;
            }

            break;
    }

    return true;
}

/**
 * @brief: Check the CRSF frame for commands that should not be passed on
 * @return: true if packet was internal and should not be processed further
*/
bool Telemetry::processInternalTelemetryPackage(uint8_t *package)
{
    const crsf_ext_header_t *header = (crsf_ext_header_t *)package;

    if (header->type == CRSF_FRAMETYPE_COMMAND)
    {
        // Non CRSF, dest=b src=l -> reboot to bootloader
        if (package[3] == 'b' && package[4] == 'l')
        {
            callBootloader = true;
            return true;
        }
        // 1. Non CRSF, dest=b src=b -> bind mode
        // 2. CRSF bind command
        if ((package[3] == 'b' && package[4] == 'd') ||
            (header->frame_size >= 6 // official CRSF is 7 bytes with two CRCs
            && header->dest_addr == CRSF_ADDRESS_CRSF_RECEIVER
            && header->orig_addr == CRSF_ADDRESS_FLIGHT_CONTROLLER
            && header->payload[0] == CRSF_COMMAND_SUBCMD_RX
            && header->payload[1] == CRSF_COMMAND_SUBCMD_RX_BIND))
        {
            callEnterBind = true;
            return true;
        }
        // Non CRSF, dest=b src=m -> set modelmatch
        if (package[3] == 'm' && package[4] == 'm')
        {
            callUpdateModelMatch = true;
            modelMatchId = package[5];
            return true;
        }
    }

    if (header->type == CRSF_FRAMETYPE_DEVICE_PING && header->dest_addr == CRSF_ADDRESS_CRSF_RECEIVER)
    {
        sendDeviceFrame = true;
        return true;
    }

    return false;
}

bool Telemetry::AppendTelemetryPackage(uint8_t *package)
{
    if (processInternalTelemetryPackage(package))
        return true;

    const crsf_header_t *header = (crsf_header_t *) package;

    if (header->type >= CRSF_FRAMETYPE_DEVICE_PING)
    {
        const crsf_ext_header_t *extHeader = (crsf_ext_header_t *) package;
        if (extHeader->orig_addr == CRSF_ADDRESS_FLIGHT_CONTROLLER)
        {
#if defined(USE_MSP_WIFI)
            // this probably needs refactoring in the future, I think we should have this telemetry class inside the crsf module
            if (wifi2tcp.hasClient() && (header->type == CRSF_FRAMETYPE_MSP_RESP || header->type == CRSF_FRAMETYPE_MSP_REQ)) // if we have a client we probs wanna talk to it
            {
                DBGLN("Got MSP frame, forwarding to client, len: %d", currentTelemetryByte);
                crsf2msp.parse(package);
            }
#endif
#if defined(HAS_MSP_VTX)
            else if (header->type == CRSF_FRAMETYPE_MSP_RESP)
            {
                mspVtxProcessPacket(package);
            }
#endif
        }
    }

    const uint8_t messageSize = CRSF_FRAME_SIZE(package[CRSF_TELEMETRY_LENGTH_INDEX]);

    // If this message has a comparator, then find out how we should handle this message
    auto action = ACTION_APPEND;
    const auto comparator = comparators.find(header->type);
    uint16_t messagePosition = 0;
    if (comparator != comparators.end())
    {
        for (uint16_t i = 0; i < messagePayloads.size();)
        {
            const auto size = messagePayloads[i];
            // If the message at this point in the queue is not deleted, and it matches this comparator, then we check it
            if (!(size & bit(7)) && messagePayloads[i + 1 + CRSF_TELEMETRY_TYPE_INDEX] == header->type)
            {
                const auto whatToDo = comparator->second == nullptr ? ACTION_OVERWRITE : comparator->second(header, messagePayloads, i + 1);
                if (whatToDo == ACTION_IGNORE || whatToDo == ACTION_APPEND || whatToDo == ACTION_OVERWRITE)
                {
                    messagePosition = i;
                    action = whatToDo;
                    break;
                }
            }
            i += 1 + (size & 0x7f);
        }
    }
    else if (isPrioritised(header->type))
    {
        settingsCount++;
    }

    messagePayloads.lock();
    switch (action)
    {
    case ACTION_IGNORE:
        break;
    case ACTION_OVERWRITE:
        if (messagePayloads[messagePosition] >= messageSize)
        {
            for (uint16_t i = 0 ; i<messageSize; i++)
            {
                messagePayloads.set(messagePosition + i + 1, package[i]);
            }
            break;
        }
        // Mark the current queued entry as deleted
        messagePayloads.set(messagePosition, messagePayloads[messagePosition] | bit(7));
        // fallthrough to APPEND
    default:
        // If there's enough room on the FIFO for this message, push it
        if (messagePayloads.available(messageSize + 1))
        {
            messagePayloads.push(messageSize);
            messagePayloads.pushBytes(package, messageSize);
        }
        else
        {
            messagePayloads.unlock();
            return false;
        }
        break;
    }
    messagePayloads.unlock();
    return true;
}

bool Telemetry::GetNextPayload(uint8_t* nextPayloadSize, uint8_t **payloadData)
{
    if (settingsCount)
    {
        // handle prioritised messages first
        for (uint16_t i = 0; i < messagePayloads.size();)
        {
            const auto size = messagePayloads[i];
            // If the message at this point in the queue is not deleted, and it's a SETTINGS_ENTRY then we're going to return it
            if (!(size & bit(7)) && isPrioritised((crsf_frame_type_e)messagePayloads[i + 1 + CRSF_TELEMETRY_TYPE_INDEX]))
            {
                settingsCount--;
                // Copy the frame to the current payload
                for (uint16_t pos = 0 ; pos < size ; pos++)
                {
                    currentPayload[pos] = messagePayloads[i + 1 + pos];
                }
                // Mark the current queued entry as deleted
                messagePayloads.set(i, size | bit(7));
                // set the pointers to the payload
                *nextPayloadSize = CRSF_FRAME_SIZE(currentPayload[CRSF_TELEMETRY_LENGTH_INDEX]);
                *payloadData = currentPayload;
                return true;
            }
            i += 1 + (size & 0x7f);
        }
        // Didn't find one, so we'll reset the counter
        settingsCount = 0;
    }

    // return the 'head' of the queue
    while (messagePayloads.size() > 0)
    {
        messagePayloads.lock();
        const auto size = messagePayloads.pop();
        if (size & bit(7))
        {
            // This message is deleted, skip it
            messagePayloads.skip(size & 0x7f);
            messagePayloads.unlock();
            continue;
        }
        messagePayloads.popBytes(currentPayload, size);
        *nextPayloadSize = CRSF_FRAME_SIZE(currentPayload[CRSF_TELEMETRY_LENGTH_INDEX]);
        *payloadData = currentPayload;
        messagePayloads.unlock();
        return true;
    }

    *nextPayloadSize = 0;
    *payloadData = nullptr;
    return false;
}

// This method is only used in unit testing!
int Telemetry::UpdatedPayloadCount()
{
    uint8_t bytes[messagePayloads.size()];
    int bytePos = 0;
    int i=0;
    while (messagePayloads.size() > 0)
    {
        auto sz = messagePayloads.pop();
        if (!(sz & bit(7)))
        {
            i++;
        }
        bytes[bytePos++] = sz;
        if (sz > 0)
        {
            while (sz--)
            {
                bytes[bytePos++] = messagePayloads.pop();
            }
        }
    }
    messagePayloads.pushBytes(bytes, bytePos);
    return i;
}

#endif
