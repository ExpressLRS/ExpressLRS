#include <cstdint>
#include <cstring>
#include <telemetry.h>
#include <crsf_protocol.h>

#if defined(UNIT_TEST)
#include <iostream>
using namespace std;
#endif
//#define TARGET_R9M_RX

#if defined(PLATFORM_ESP8266) || defined(TARGET_R9M_RX) || defined(UNIT_TEST)

telemetry_state_s Telemetry::telemtry_state = TELEMTRY_IDLE;
uint8_t Telemetry::CRSFinBuffer[CRSF_MAX_PACKET_LEN] = {0};
uint8_t Telemetry::currentTelemetryByte = 0;
uint8_t Telemetry::currentPayloadIndex = 0;
bool Telemetry::callBootloader = false;
uint8_t Telemetry::receivedPackages = 0;
PAYLOAD_DATA(GPS, BATTERY_SENSOR, ATTITUDE, DEVICE_INFO, FLIGHT_MODE);

uint8_t* Telemetry::GetNextPayload()
{
    uint8_t checks = 0;
    uint8_t oldPayloadIndex = currentPayloadIndex;

    payloadTypes[currentPayloadIndex].locked = false;
    payloadTypes[currentPayloadIndex].updated = false;

    do
    {
        currentPayloadIndex = (currentPayloadIndex + 1) % payloadTypesCount;
        checks++;
    } while(!payloadTypes[currentPayloadIndex].updated && checks < payloadTypesCount);

    if (payloadTypes[currentPayloadIndex].updated)
    {
        payloadTypes[currentPayloadIndex].locked = true;
        return payloadTypes[currentPayloadIndex].data;
    }

    currentPayloadIndex = oldPayloadIndex;
    return 0;
}

uint8_t* Telemetry::GetCurrentPayload()
{
    if (payloadTypes[currentPayloadIndex].updated)
    {
        return payloadTypes[currentPayloadIndex].data;
    }

    return 0;
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
    telemtry_state = TELEMTRY_IDLE;
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
    switch(telemtry_state) {
        case TELEMTRY_IDLE:
            if (data == CRSF_ADDRESS_CRSF_RECEIVER || data == CRSF_SYNC_BYTE)
            {
                currentTelemetryByte = 0;
                telemtry_state = RECEIVING_LEGNTH;
                CRSFinBuffer[0] = data;
            }
            else {
                return false;
            }

            break;
        case RECEIVING_LEGNTH:
            if (data >= CRSF_MAX_PACKET_LEN)
            {
                telemtry_state = TELEMTRY_IDLE;
                return false;
            }
            else
            {
                telemtry_state = RECEIVING_DATA;
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
                telemtry_state = TELEMTRY_IDLE;

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

void Telemetry::AppendTelemetryPackage()
{
    if (CRSFinBuffer[CRSF_TELEMETRY_TYPE_INDEX] == CRSF_FRAMETYPE_COMMAND && CRSFinBuffer[3] == 0x62 && CRSFinBuffer[4] == 0x6c)
    {
        callBootloader = true;
        return;
    }

    for (int8_t i = 0; i < payloadTypesCount; i++)
    {
        if (CRSFinBuffer[CRSF_TELEMETRY_TYPE_INDEX] == payloadTypes[i].type && !payloadTypes[i].locked)
        {
            if (CRSF_FRAME_SIZE(CRSFinBuffer[CRSF_TELEMETRY_LENGTH_INDEX]) <= payloadTypes[i].size)
            {
                memcpy(payloadTypes[i].data, CRSFinBuffer, CRSF_FRAME_SIZE(CRSFinBuffer[CRSF_TELEMETRY_LENGTH_INDEX]));
                payloadTypes[i].updated = true;
            }
            #if defined(UNIT_TEST)
            else
            {
                cout << "buffer not large enough for type " << (int)payloadTypes[i].type  << " with size " << (int)payloadTypes[i].size << " would need " << CRSF_FRAME_SIZE(CRSFinBuffer[CRSF_TELEMETRY_LENGTH_INDEX]) << '\n';
            }
            #endif
            return;
        }
    }
}
#endif
