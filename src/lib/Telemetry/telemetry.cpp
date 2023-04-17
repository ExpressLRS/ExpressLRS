#include <cstdint>
#include <cstring>
#include "telemetry.h"
#include "logging.h"

#if defined(USE_MSP_WIFI) && defined(TARGET_RX) // enable MSP2WIFI for RX only at the moment
#include "tcpsocket.h"
extern TCPSOCKET wifi2tcp;
#endif

#if defined(UNIT_TEST)
#include <iostream>
using namespace std;
#endif

#if CRSF_RX_MODULE

#include "crsf2msp.h"

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

void Telemetry::CheckCrsfBatterySensorDetected()
{
    if (CRSFinBuffer[CRSF_TELEMETRY_TYPE_INDEX] == CRSF_FRAMETYPE_BATTERY_SENSOR)
    {
        crsfBatterySensorDetected = true;
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
                uint8_t crc = crsf_crc.calc(CRSFinBuffer + CRSF_FRAME_NOT_COUNTED_BYTES, CRSFinBuffer[CRSF_TELEMETRY_LENGTH_INDEX] - CRSF_TELEMETRY_CRC_LENGTH);
                telemetry_state = TELEMETRY_IDLE;

                if (data == crc)
                {
                    AppendTelemetryPackage(CRSFinBuffer);

                    // Special case to check here and not in AppendTelemetryPackage().  devAnalogVbat sends
                    // direct to AppendTelemetryPackage() and we want to detect packets only received through serial.
                    CheckCrsfBatterySensorDetected();

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

void Telemetry::AppendTelemetryPackage(uint8_t *package)
{
    const crsf_ext_header_t *header = (crsf_ext_header_t *) package;

    //handle non-OTA
    if (header->type == CRSF_FRAMETYPE_COMMAND && package[3] == 'b' && package[4] == 'l') // note: this should be an extended csrf packet, not a standard crsf packet
    {
        callBootloader = true;
        return;
    }
    if (header->type == CRSF_FRAMETYPE_COMMAND && package[3] == 'b' && package[4] == 'd') // note: this should be an extended csrf packet, not a standard crsf packet
    {
        callEnterBind = true;
        return;
    }
    if (header->type == CRSF_FRAMETYPE_COMMAND && package[3] == 'm' && package[4] == 'm') // note: this should be an extended csrf packet, not a standard crsf packet
    {
        callUpdateModelMatch = true;
        modelMatchId = package[5];
        return;
    }
    if (header->type == CRSF_FRAMETYPE_DEVICE_PING && header->dest_addr == CRSF_ADDRESS_CRSF_RECEIVER)
    {
        sendDeviceFrame = true;
        return;
    }
    #if defined(USE_MSP_WIFI) && defined(TARGET_RX)
    if (header->type >= CRSF_FRAMETYPE_DEVICE_PING && header->orig_addr == CRSF_ADDRESS_FLIGHT_CONTROLLER)
    {
        if (wifi2tcp.hasClient() && (header->type == CRSF_FRAMETYPE_MSP_RESP || header->type == CRSF_FRAMETYPE_MSP_REQ)) // if we have a client we probs wanna talk to it
        {
            DBGLN("Got MSP frame, forwarding to client, len: %d", currentTelemetryByte);
            crsf2msp.parse(package);
        }
        //fall through: if no TCP client we just want to forward MSP over the link
    }
    #endif

    //handle OTA
    uint8_t targetIndex = 0;
    bool targetFound = false;

    //store crsf telemetry in 1-slot LIFO buffer, overwrite exising data (if the crsf package fits in the buffer)
    for (int8_t i = 0; i < payloadTypesCount - 2; i++)
    {
        if (header->type == payloadTypes[i].type && CRSF_FRAME_SIZE(header->frame_size) <= payloadTypes[i].size)
        {
            targetIndex = i;
            targetFound = true;
            break;
        }
    }

    // store anything else in 2-slot FIFO buffer, do not overwrite existing data. This also handles larger msp responses, which are sent in two chunks
    if (!targetFound)
    {
        // first try slot payloadTypesCount - 2, so that the OTA packets are transmitted in the same order as they are received
        targetIndex = payloadTypesCount - 2;
        targetFound = !payloadTypes[targetIndex].updated || !(header->orig_addr == CRSF_ADDRESS_FLIGHT_CONTROLLER || (header->type == CRSF_FRAMETYPE_ARDUPILOT_RESP && package[CRSF_TELEMETRY_TYPE_INDEX + 1] == CRSF_AP_CUSTOM_TELEM_STATUS_TEXT));
        if (!targetFound)
        {
            // use other slot if first slot is full
            targetIndex = payloadTypesCount - 1;
            targetFound = !payloadTypes[targetIndex].updated;
        }
    }

    // write to slot, but only if the slot is not locked by the OTA sender
    if (targetFound && !payloadTypes[targetIndex].locked)
    {
        memcpy(payloadTypes[targetIndex].data, package, CRSF_FRAME_SIZE(package[CRSF_TELEMETRY_LENGTH_INDEX]));
        payloadTypes[targetIndex].updated = true;
    }
}
#endif
