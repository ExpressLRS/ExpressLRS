#include "CRSFParser.h"

#include "CRSFRouter.h"

bool CRSFParser::processByte(CRSFConnector *origin, const uint8_t byte, const std::function<void(const crsf_header_t *)>& foundMessage)
{
    switch(telemetry_state) {
        case TELEMETRY_IDLE:
            // Telemetry from Betaflight/iNav starts with CRSF_SYNC_BYTE (CRSF_ADDRESS_FLIGHT_CONTROLLER)
            // from a TX module it will be addressed to CRSF_ADDRESS_RADIO_TRANSMITTER (RX used as a relay)
            // and things addressed to CRSF_ADDRESS_CRSF_RECEIVER I guess we should take too since that's us, but we'll just forward them
            if (byte == CRSF_SYNC_BYTE || byte == CRSF_ADDRESS_RADIO_TRANSMITTER || byte == CRSF_ADDRESS_CRSF_RECEIVER)
            {
                currentTelemetryByte = 0;
                telemetry_state = RECEIVING_LENGTH;
                CRSFinBuffer[0] = byte;
            }
            else
            {
                return false;
            }
            break;

        case RECEIVING_LENGTH:
            if (byte >= CRSF_MAX_PACKET_LEN)
            {
                telemetry_state = TELEMETRY_IDLE;
                return false;
            }
            telemetry_state = RECEIVING_DATA;
            CRSFinBuffer[CRSF_TELEMETRY_LENGTH_INDEX] = byte;
            break;

        case RECEIVING_DATA:
            CRSFinBuffer[currentTelemetryByte + CRSF_FRAME_NOT_COUNTED_BYTES] = byte;
            currentTelemetryByte++;
            if (CRSFinBuffer[CRSF_TELEMETRY_LENGTH_INDEX] == currentTelemetryByte)
            {
                // exclude first bytes (sync byte + length), skip last byte (submitted crc)
                uint8_t crc = crsfRouter.crsf_crc.calc(CRSFinBuffer + CRSF_FRAME_NOT_COUNTED_BYTES, CRSFinBuffer[CRSF_TELEMETRY_LENGTH_INDEX] - CRSF_TELEMETRY_CRC_LENGTH);
                telemetry_state = TELEMETRY_IDLE;

                if (byte == crc)
                {
                    const crsf_header_t *header = (crsf_header_t *) CRSFinBuffer;
                    crsfRouter.processMessage(origin, header);
                    // We have found a packet
                    if (foundMessage) foundMessage(header);
                    return true;
                }
                return false;
            }
            break;
    }

    return true;
}
