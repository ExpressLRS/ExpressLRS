#include "telemetry.h"
#include <cstring>

#if defined(TARGET_RX) || defined(UNIT_TEST)
#if defined(UNIT_TEST)
#include <iostream>
using namespace std;
#endif

#include "CRSFRouter.h"

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

PAYLOAD_DATA(GPS, BATTERY_SENSOR, ATTITUDE, DEVICE_INFO, FLIGHT_MODE, VARIO, BARO_ALTITUDE);

bool Telemetry::GetNextPayload(uint8_t* nextPayloadSize, uint8_t **payloadData)
{
    uint8_t checks = 0;
    uint8_t oldPayloadIndex = currentPayloadIndex;
    uint8_t realLength = 0;

    if (payloadTypes[currentPayloadIndex].locked)
    {
        payloadTypes[currentPayloadIndex].locked = false;
        payloadTypes[currentPayloadIndex].updated = false;
    }

    do
    {
        currentPayloadIndex = (currentPayloadIndex + 1) % payloadTypesCount;
        checks++;
    } while(!payloadTypes[currentPayloadIndex].updated && checks < payloadTypesCount);

    if (payloadTypes[currentPayloadIndex].updated)
    {
        payloadTypes[currentPayloadIndex].locked = true;

        realLength = CRSF_FRAME_SIZE(payloadTypes[currentPayloadIndex].data[CRSF_TELEMETRY_LENGTH_INDEX]);
        if (realLength > 0)
        {
            *nextPayloadSize = realLength;
            *payloadData = payloadTypes[currentPayloadIndex].data;
            return true;
        }
    }

    currentPayloadIndex = oldPayloadIndex;
    *nextPayloadSize = 0;
    *payloadData = 0;
    return false;
}

uint8_t Telemetry::UpdatedPayloadCount()
{
    uint8_t count = 0;
    for (int8_t i = 0; i < payloadTypesCount; i++)
    {
        if (payloadTypes[i].updated)
        {
            count++;
        }
    }

    return count;
}

uint8_t Telemetry::ReceivedPackagesCount()
{
    return receivedPackages;
}

void Telemetry::ResetState()
{
    telemetry_state = TELEMETRY_IDLE;
    currentTelemetryByte = 0;
    currentPayloadIndex = 0;
    twoslotLastQueueIndex = 0;
    receivedPackages = 0;

    uint8_t offset = 0;

    for (int8_t i = 0; i < payloadTypesCount; i++)
    {
        payloadTypes[i].locked = false;
        payloadTypes[i].updated = false;
        payloadTypes[i].data = PayloadData + offset;
        offset += payloadTypes[i].size;

        #if defined(UNIT_TEST)
        if (offset > sizeof(PayloadData)) {
            cout << "data not large enough\n";
        }
        #endif
    }
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

                    receivedPackages++;
                    return true;
                }
                #if defined(UNIT_TEST)
                if (data != crc)
                {
                    cout << "invalid " << (int)crc  << '\n';
                }
                #endif

                return false;
            }

            break;
    }

    return true;
}

bool Telemetry::AppendTelemetryPackage(uint8_t *package)
{
    const crsf_header_t *header = (crsf_header_t *) package;
    uint8_t targetIndex = 0;
    bool targetFound = false;

    if (header->type >= CRSF_FRAMETYPE_DEVICE_PING)
    {
        const crsf_ext_header_t *extHeader = (crsf_ext_header_t *) package;

        if (header->type == CRSF_FRAMETYPE_ARDUPILOT_RESP)
        {
            // reserve last slot for adrupilot custom frame with the sub type status text: this is needed to make sure the important status messages are not lost
            if (package[CRSF_TELEMETRY_TYPE_INDEX + 1] == CRSF_AP_CUSTOM_TELEM_STATUS_TEXT)
            {
                targetIndex = payloadTypesCount - 1;
            }
            else
            {
                targetIndex = payloadTypesCount - 2;
            }
            targetFound = true;
        }
        else if (extHeader->orig_addr == CRSF_ADDRESS_FLIGHT_CONTROLLER)
        {
            targetIndex = payloadTypesCount - 2;
            targetFound = true;

            // This code is emulating a two slot FIFO with head dropping
            if (currentPayloadIndex == payloadTypesCount - 2 && payloadTypes[currentPayloadIndex].locked)
            {
                // Sending the first slot, use the second
                targetIndex = payloadTypesCount - 1;
            }
            else if (currentPayloadIndex == payloadTypesCount - 1 && payloadTypes[currentPayloadIndex].locked)
            {
                // Sending the second slot, use the first
                targetIndex = payloadTypesCount - 2;
            }
            else if (twoslotLastQueueIndex == payloadTypesCount - 2 && payloadTypes[twoslotLastQueueIndex].updated)
            {
                // Previous frame saved to the first slot, use the second
                targetIndex = payloadTypesCount - 1;
            }
            twoslotLastQueueIndex = targetIndex;
        }
        else
        {
            targetIndex = payloadTypesCount - 1;
            targetFound = true;
        }
    }
    else
    {
        for (int8_t i = 0; i < payloadTypesCount - 2; i++)
        {
            if (header->type == payloadTypes[i].type)
            {
                if (CRSF_FRAME_SIZE(package[CRSF_TELEMETRY_LENGTH_INDEX]) <= payloadTypes[i].size)
                {
                    targetIndex = i;
                    targetFound = true;
                }
                #if defined(UNIT_TEST)
                else if (CRSF_FRAME_SIZE(package[CRSF_TELEMETRY_LENGTH_INDEX]) > payloadTypes[i].size)
                {
                    cout << "buffer not large enough for type " << (int)payloadTypes[i].type  << " with size " << (int)payloadTypes[i].size << " would need " << CRSF_FRAME_SIZE(package[CRSF_TELEMETRY_LENGTH_INDEX]) << '\n';
                }
                #endif
                break;
            }
        }
    }

    if (targetFound && !payloadTypes[targetIndex].locked)
    {
        memcpy(payloadTypes[targetIndex].data, package, CRSF_FRAME_SIZE(package[CRSF_TELEMETRY_LENGTH_INDEX]));
        payloadTypes[targetIndex].updated = true;
    }

    return targetFound;
}
#endif
