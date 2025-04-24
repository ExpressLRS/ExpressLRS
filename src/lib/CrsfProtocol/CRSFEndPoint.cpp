#include "CRSFEndPoint.h"
#include "msptypes.h"

extern GENERIC_CRC8 crsf_crc;

void CRSFEndPoint::addConnector(CRSFConnector *connector)
{
    connectors.push_back(connector);
}

void CRSFEndPoint::processMessage(CRSFConnector *connector, crsf_ext_header_t *message)
{
    if (connector) connector->addDevice(message->orig_addr);

    const crsf_frame_type_e packetType = message->type;
    if (packetType < CRSF_FRAMETYPE_DEVICE_PING || (packetType >= CRSF_FRAMETYPE_DEVICE_PING && (message->dest_addr == device_id || message->dest_addr == CRSF_ADDRESS_BROADCAST)))
    {
        const auto eat_message = handleMessage(message);
        if (eat_message)
        {
            return;
        }
    }

    if (packetType < CRSF_FRAMETYPE_DEVICE_PING || message->dest_addr == CRSF_ADDRESS_BROADCAST || message->dest_addr != device_id)
    {
        // forward the message to all other connectors and let them figure out if they want to deliver it
        for (const auto other : connectors)
        {
            if (other != connector) other->processMessage(message);
        }
    }
}

void CRSFEndPoint::AddMspMessage(mspPacket_t* packet, uint8_t destination)
{
    if (packet->payloadSize > ENCAPSULATED_MSP_MAX_PAYLOAD_SIZE)
    {
        return;
    }

    const uint8_t totalBufferLen = packet->payloadSize + ENCAPSULATED_MSP_HEADER_CRC_LEN + CRSF_FRAME_LENGTH_EXT_TYPE_CRC + CRSF_FRAME_NOT_COUNTED_BYTES;
    uint8_t outBuffer[ENCAPSULATED_MSP_MAX_FRAME_LEN + CRSF_FRAME_LENGTH_EXT_TYPE_CRC + CRSF_FRAME_NOT_COUNTED_BYTES];

    // CRSF extended frame header
    outBuffer[0] = CRSF_ADDRESS_BROADCAST;                                      // address
    outBuffer[1] = packet->payloadSize + ENCAPSULATED_MSP_HEADER_CRC_LEN + CRSF_FRAME_LENGTH_EXT_TYPE_CRC; // length
    outBuffer[2] = CRSF_FRAMETYPE_MSP_WRITE;                                    // packet type
    outBuffer[3] = destination;                                                 // destination
    outBuffer[4] = CRSF_ADDRESS_RADIO_TRANSMITTER;                              // origin

    // Encapsulated MSP payload
    outBuffer[5] = 0x30;                // header
    outBuffer[6] = packet->payloadSize; // mspPayloadSize
    outBuffer[7] = packet->function;    // packet->cmd
    for (uint16_t i = 0; i < packet->payloadSize; ++i)
    {
        // copy packet payload into outBuffer
        outBuffer[8 + i] = packet->payload[i];
    }
    // Encapsulated MSP crc
    outBuffer[totalBufferLen - 2] = CalcCRCMsp(&outBuffer[6], packet->payloadSize + 2);

    // CRSF frame crc
    outBuffer[totalBufferLen - 1] = crsf_crc.calc(&outBuffer[2], packet->payloadSize + ENCAPSULATED_MSP_HEADER_CRC_LEN + CRSF_FRAME_LENGTH_EXT_TYPE_CRC - 1);
    processMessage(nullptr, (crsf_ext_header_t *)outBuffer);
}

