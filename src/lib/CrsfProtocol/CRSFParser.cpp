#include "CRSFParser.h"

#include "CRSFRouter.h"

void CRSFParser::processBytes(CRSFConnector *origin, const uint8_t *inputBytes, const uint16_t size, const std::function<void(const crsf_header_t *)>& foundMessage)
{
    for (uint8_t i = 0; i < size; ++i)
    {
        processByte(origin, inputBytes[i], foundMessage);
    }
}

bool CRSFParser::processByte(CRSFConnector *origin, const uint8_t inputByte, const std::function<void(const crsf_header_t *)>& foundMessage)
{
    switch(telemetry_state) {
        case TELEMETRY_IDLE:
            // Telemetry from Betaflight/iNav starts with CRSF_SYNC_BYTE (CRSF_ADDRESS_FLIGHT_CONTROLLER)
            // from a TX module it will be addressed to CRSF_ADDRESS_RADIO_TRANSMITTER (RX used as a relay)
            // and things addressed to CRSF_ADDRESS_CRSF_RECEIVER I guess we should take too since that's us, but we'll just forward them
            if (inputByte == CRSF_SYNC_BYTE || inputByte == CRSF_ADDRESS_RADIO_TRANSMITTER || inputByte == CRSF_ADDRESS_CRSF_RECEIVER)
            {
                inBufferIndex = 0;
                telemetry_state = RECEIVING_LENGTH;
                CRSFinBuffer[0] = inputByte;
            }
            else
            {
                return false;
            }
            break;

        case RECEIVING_LENGTH:
            if (inputByte >= CRSF_MAX_PACKET_LEN)
            {
                telemetry_state = TELEMETRY_IDLE;
                return false;
            }
            telemetry_state = RECEIVING_DATA;
            CRSFinBuffer[CRSF_TELEMETRY_LENGTH_INDEX] = inputByte;
            break;

        case RECEIVING_DATA:
            CRSFinBuffer[inBufferIndex + CRSF_FRAME_NOT_COUNTED_BYTES] = inputByte;
            inBufferIndex++;
            if (CRSFinBuffer[CRSF_TELEMETRY_LENGTH_INDEX] == inBufferIndex)
            {
                // exclude first bytes (sync byte + length), skip last byte (submitted crc)
                const uint8_t crc = crsfRouter.crsf_crc.calc(CRSFinBuffer + CRSF_FRAME_NOT_COUNTED_BYTES, CRSFinBuffer[CRSF_TELEMETRY_LENGTH_INDEX] - CRSF_TELEMETRY_CRC_LENGTH);
                telemetry_state = TELEMETRY_IDLE;

                if (inputByte == crc)
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
