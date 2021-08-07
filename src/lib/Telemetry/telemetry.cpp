#include <cstdint>
#include <cstring>
#include "telemetry.h"

#if defined(UNIT_TEST)
#include <iostream>
using namespace std;
#endif

#if CRSF_RX_MODULE

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

PAYLOAD_DATA(GPS, BATTERY_SENSOR, ATTITUDE, DEVICE_INFO, FLIGHT_MODE, MSP_RESP);

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
        // search for non zero data from the end
        while (realLength > 0 && payloadTypes[currentPayloadIndex].data[realLength - 1] == 0)
        {
            realLength--;
        }

        if (realLength > 0)
        {
            // store real length in frame
            payloadTypes[currentPayloadIndex].data[CRSF_TELEMETRY_LENGTH_INDEX] = realLength - CRSF_FRAME_NOT_COUNTED_BYTES;
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

bool Telemetry::RXhandleUARTin(uint8_t data)
{
    switch(telemetry_state) {
        case TELEMETRY_IDLE:
            if (data == CRSF_ADDRESS_CRSF_RECEIVER || data == CRSF_SYNC_BYTE)
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
                uint8_t crc = CalcCRC(CRSFinBuffer + CRSF_FRAME_NOT_COUNTED_BYTES, CRSFinBuffer[CRSF_TELEMETRY_LENGTH_INDEX] - CRSF_TELEMETRY_CRC_LENGTH);
                telemetry_state = TELEMETRY_IDLE;

                if (data == crc)
                {
                    AppendTelemetryPackage();
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

int Telemetry::findPayloadType(uint8_t type)
{
    for (int i = 0; i < payloadTypesCount; i++)
        if (type == payloadTypes[i].type)
            return i;
    return -1;
}

/**
 * Return the low byte of the VBatt sensor, up to 25.6V
 **/
uint8_t Telemetry::getVBattSensorLow()
{
    int idx = findPayloadType(CRSF_FRAMETYPE_BATTERY_SENSOR);
    if (idx == -1)
        return 0;

    return payloadTypes[idx].data[CRSF_TELEMETRY_TYPE_INDEX+2];
}

void Telemetry::AppendTelemetryPackage()
{
    if (CRSFinBuffer[CRSF_TELEMETRY_TYPE_INDEX] == CRSF_FRAMETYPE_COMMAND && CRSFinBuffer[3] == 'b' && CRSFinBuffer[4] == 'l')
    {
        callBootloader = true;
        return;
    }
    if (CRSFinBuffer[CRSF_TELEMETRY_TYPE_INDEX] == CRSF_FRAMETYPE_COMMAND && CRSFinBuffer[3] == 'b' && CRSFinBuffer[4] == 'd')
    {
        callEnterBind = true;
        return;
    }

    int idx = findPayloadType(CRSFinBuffer[CRSF_TELEMETRY_TYPE_INDEX]);
    if (idx != -1 && !payloadTypes[idx].locked)
    {
        if (CRSF_FRAME_SIZE(CRSFinBuffer[CRSF_TELEMETRY_LENGTH_INDEX]) <= payloadTypes[idx].size)
        {
            memcpy(payloadTypes[idx].data, CRSFinBuffer, CRSF_FRAME_SIZE(CRSFinBuffer[CRSF_TELEMETRY_LENGTH_INDEX]));
            payloadTypes[idx].updated = true;
        }
        #if defined(UNIT_TEST)
        else
        {
            cout << "buffer not large enough for type " << (int)payloadTypes[idx].type  << " with size " << (int)payloadTypes[idx].size << " would need " << CRSF_FRAME_SIZE(CRSFinBuffer[CRSF_TELEMETRY_LENGTH_INDEX]) << '\n';
        }
        #endif
    }
}
#endif
