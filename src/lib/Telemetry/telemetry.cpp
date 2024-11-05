#include <cstdint>
#include <cstring>
#include "telemetry.h"
#include "logging.h"

#if defined(TARGET_RX) // enable MSP2WIFI for RX only at the moment
#include "tcpsocket.h"
extern TCPSOCKET wifi2tcp;
#endif

#if defined(TARGET_RX) || defined(UNIT_TEST)
#include "devMSPVTX.h"

#if defined(UNIT_TEST)
#include <iostream>
using namespace std;
#endif

#include "crsf2msp.h"
typedef struct crsf_sensor_gps_s
{
    int32_t latitude;     // degree / 10,000,000 big endian
    int32_t longitude;    // degree / 10,000,000 big endian
    uint16_t groundspeed; // km/h / 10 big endian
    uint16_t heading;     // GPS heading, degree/100 big endian
    uint16_t altitude;    // meters, +1000m big endian
    uint8_t satellites;   // satellites
} PACKED crsf_sensor_gps_t;

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

#if ( defined(RADIO_SX127X) && defined(TARGET_RX) )
void Telemetry::CheckCrsfGPSSensorDetected()
{
    if (CRSFinBuffer[CRSF_TELEMETRY_TYPE_INDEX] == CRSF_FRAMETYPE_GPS)
    {
        uint8_t sats = CRSFinBuffer[17];

        if ( (sats >=4) && (sats <= 99) )
        {
            beaconSats = sats;
            beaconLat = (uint32_t(CRSFinBuffer[ 3])<<24) + (uint32_t(CRSFinBuffer[ 4])<<16) + (uint32_t(CRSFinBuffer[ 5])<<8) + CRSFinBuffer[6]; 
            beaconLon = (uint32_t(CRSFinBuffer[ 7])<<24) + (uint32_t(CRSFinBuffer[ 8])<<16) + (uint32_t(CRSFinBuffer[ 9])<<8) + CRSFinBuffer[10]; 
            beaconSpd = (uint16_t(CRSFinBuffer[11])<<8) + CRSFinBuffer[12];
            beaconHdg = (uint16_t(CRSFinBuffer[13])<<8) + CRSFinBuffer[14];
            beaconAlt = (uint16_t(CRSFinBuffer[15])<<8) + CRSFinBuffer[16];
        }
    }
}
#endif

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

                    #if ( defined(RADIO_SX127X) && defined(TARGET_RX) )
                    CheckCrsfGPSSensorDetected();
                    #endif

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

#if defined(TARGET_RX)
            // this probably needs refactoring in the future, I think we should have this telemetry class inside the crsf module
            if (wifi2tcp.hasClient() && (header->type == CRSF_FRAMETYPE_MSP_RESP || header->type == CRSF_FRAMETYPE_MSP_REQ)) // if we have a client we probs wanna talk to it
            {
                DBGLN("Got MSP frame, forwarding to client, len: %d", currentTelemetryByte);
                crsf2msp.parse(package);
            }
            else // if no TCP client we just want to forward MSP over the link
#endif
            {
#if defined(PLATFORM_ESP32)
                if (header->type == CRSF_FRAMETYPE_MSP_RESP)
                {
                    mspVtxProcessPacket(package);
                }
#endif
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

#if ( defined(TARGET_RX) && defined(RADIO_SX127X) )
/**
 * @brief: Sends last good GPS CRSF frame received from FC
 * @return: nothing
*/
void Telemetry::SendLastGoodGPS()
{
    static uint16_t counter = 0;
    if (beaconSats > 0)
    {
        CRSF_MK_FRAME_T(crsf_sensor_gps_t)
        crsfGPS = {0};
        crsfGPS.p.latitude = htobe32(beaconLat);
        crsfGPS.p.longitude = htobe32(beaconLon);
        crsfGPS.p.groundspeed = htobe16(beaconSpd); // ELRS 1 = 0.1km/h
        // crsfGPS.p.heading = htobe16(1 * 100);
        crsfGPS.p.heading = htobe16(counter * 100); // rotating heading to indicate we're using stored data
        crsfGPS.p.altitude = htobe16(beaconAlt); // 0m = 1000 - do not offset - we already store it with offset
        crsfGPS.p.satellites = beaconSats&0b01111111;  // undo the bit flag indicating the old packet - elrs doesn't know how to use it
        
        CRSF::SetHeaderAndCrc((uint8_t *)&crsfGPS, CRSF_FRAMETYPE_GPS, CRSF_FRAME_SIZE(sizeof(crsf_sensor_gps_t)), CRSF_ADDRESS_CRSF_TRANSMITTER);
        AppendTelemetryPackage((uint8_t *)&crsfGPS);
        counter = (counter+1)%360;
    }
}
#endif

#endif
