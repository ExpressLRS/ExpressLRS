#include <cstdint>
#include <cstring>
#include "telemetry.h"
#include "logging.h"
#include "helpers.h"

#if CRSF_RX_MODULE

#if defined(USE_MSP_WIFI) // enable MSP2WIFI for RX only at the moment
#include "tcpsocket.h"
extern TCPSOCKET wifi2tcp;
#endif

#if defined(HAS_MSP_VTX)
#include "devMSPVTX.h"
#endif

#if defined(UNIT_TEST)
#include <iostream>
using namespace std;
#endif

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

    uint32_t offset = 0;

    usedSingletons = 0;
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

    const uint8_t messageSize = CRSF_FRAME_SIZE(((uint8_t *)package)[CRSF_TELEMETRY_LENGTH_INDEX]);

    messagePayloads.lock();
    // If this message is a 'singleton', then overwrite the bucket and add it to the sending queue if it's not already there
    for (int8_t i = 0; i < (int8_t)ARRAY_SIZE(singletonPayloadTypes); i++)
    {
        if (header->type == singletonPayloadTypes[i])
        {
            // Special case the Ardupilot status text message, which we also treat as a latest-singleton
            if (header->type != CRSF_FRAMETYPE_ARDUPILOT_RESP || package[CRSF_TELEMETRY_TYPE_INDEX + 1] == CRSF_AP_CUSTOM_TELEM_STATUS_TEXT)
            {
                memcpy(singletonPayloads[i], package, messageSize);
                // Add to the packetSource queue if not already on it
                if ((usedSingletons & 1 << i) == 0 && messagePayloads.available(1))
                {
                    usedSingletons |= 1 << i;
                    messagePayloads.push(-(i + 1));
                }
                messagePayloads.unlock();
                return true;
            }
        }
    }

    // IF there's enough room on the FIFO for this message, push it
    if (messagePayloads.available(messageSize + 1))
    {
        messagePayloads.push(messageSize);
        messagePayloads.pushBytes(package, messageSize);
    }
    messagePayloads.unlock();
    return true;
}

bool Telemetry::GetNextPayload(uint8_t* nextPayloadSize, uint8_t **payloadData)
{
    if (messagePayloads.size() == 0)
    {
        *nextPayloadSize = 0;
        *payloadData = nullptr;
        return false;
    }

    messagePayloads.lock();
    const auto byte = (int8_t)messagePayloads.pop();
    if (byte < 0) {
        const int fixedIndex = -byte - 1;
        *nextPayloadSize = CRSF_FRAME_SIZE(singletonPayloads[fixedIndex][CRSF_TELEMETRY_LENGTH_INDEX]);
        memcpy(currentPayload, singletonPayloads[fixedIndex], *nextPayloadSize);
        *payloadData = currentPayload;
        usedSingletons &= ~(1 << fixedIndex); // remove from the usedSingletons set
    }
    else
    {
        *nextPayloadSize = byte;
        messagePayloads.popBytes(currentPayload, *nextPayloadSize);
        *payloadData = currentPayload;
    }
    messagePayloads.unlock();
    return true;
}

// This method is only used in unit testing!
int Telemetry::UpdatedPayloadCount()
{
    uint8_t bytes[messagePayloads.size()];
    int bytePos = 0;
    int i=0;
    while (messagePayloads.size() > 0)
    {
        i++;
        auto sz = (int8_t)messagePayloads.pop();
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
