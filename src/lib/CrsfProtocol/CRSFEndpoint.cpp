#include "CRSFEndpoint.h"

#include "msptypes.h"

void CRSFEndpoint::addConnector(CRSFConnector *connector)
{
    connectors.push_back(connector);
}

void CRSFEndpoint::processMessage(CRSFConnector *connector, const crsf_header_t *message)
{
    if (handleRaw(message))
    {
        return;
    }

    const crsf_frame_type_e packetType = message->type;
    const auto extMessage = (crsf_ext_header_t *)message;
    if (connector && packetType >= CRSF_FRAMETYPE_DEVICE_PING)
    {
        connector->addDevice(extMessage->orig_addr);
    }

    if (packetType < CRSF_FRAMETYPE_DEVICE_PING || extMessage->dest_addr == device_id || extMessage->dest_addr == CRSF_ADDRESS_BROADCAST)
    {
        handleMessage(message);
        if (packetType >= CRSF_FRAMETYPE_DEVICE_PING && extMessage->dest_addr == device_id)
        {
            return;
        }
    }

    // deliver extended header messages to the connector that 'knows' about the destination device address
    if (packetType >= CRSF_FRAMETYPE_DEVICE_PING && extMessage->dest_addr != CRSF_ADDRESS_BROADCAST)
    {
        for (const auto other : connectors)
        {
            if (other != connector && other->forwardsTo(extMessage->dest_addr))
            {
                other->forwardMessage(message);
                return;
            }
        }
    }

    // The destination is not known, or it's a broadcast message so deliver to all other connectors
    for (const auto other : connectors)
    {
        if (other != connector)
        {
            other->forwardMessage(message);
        }
    }
}

void CRSFEndpoint::SetMspV2Request(uint8_t *frame, uint16_t function, uint8_t *payload, uint8_t payloadLength)
{
    auto *packet = frame + sizeof(crsf_ext_header_t);
    packet[0] = 0x50;          // no error, version 2, beginning of the frame, first frame (0)
    packet[1] = 0;             // flags
    packet[2] = function & 0xFF;
    packet[3] = (function >> 8) & 0xFF;
    packet[4] = payloadLength & 0xFF;
    packet[5] = (payloadLength >> 8) & 0xFF;
    memcpy(packet + 6, payload, payloadLength);
    packet[6 + payloadLength] = CalcCRCMsp(packet + 1, payloadLength + 5); // crc = flags + function + length + payload
}

void CRSFEndpoint::SetHeaderAndCrc(crsf_header_t *frame, crsf_frame_type_e frameType, uint8_t frameSize, crsf_addr_e destAddr)
{
    frame->device_addr = destAddr;
    frame->frame_size = frameSize;
    frame->type = frameType;

    uint8_t crc = crsf_crc.calc((uint8_t *)frame + CRSF_FRAME_NOT_COUNTED_BYTES, frameSize - 1, 0);
    ((uint8_t*)frame)[frameSize + CRSF_FRAME_NOT_COUNTED_BYTES - 1] = crc;
}

void CRSFEndpoint::SetExtendedHeaderAndCrc(crsf_ext_header_t *frame, const crsf_frame_type_e frameType, const uint8_t frameSize, const crsf_addr_e destAddr)
{
    frame->dest_addr = destAddr;
    frame->orig_addr = device_id;
    SetHeaderAndCrc((crsf_header_t *)frame, frameType, frameSize, destAddr);
}

void CRSFEndpoint::makeLinkStatisticsPacket(uint8_t *buffer, crsf_addr_e destination)
{
    // Note: size of crsfLinkStatistics_t used, not full elrsLinkStatistics_t
    constexpr uint8_t payloadLen = sizeof(crsfLinkStatistics_t);

    buffer[0] = destination;
    buffer[1] = CRSF_FRAME_SIZE(payloadLen);
    buffer[2] = CRSF_FRAMETYPE_LINK_STATISTICS;
    memcpy(&buffer[3], &linkStats, payloadLen);
    buffer[payloadLen + 3] = crsf_crc.calc(&buffer[2], payloadLen + 1);
}

void CRSFEndpoint::AddMspMessage(const mspPacket_t * packet, const uint8_t destination, const uint8_t origin)
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
    outBuffer[4] = origin;                                                      // origin

    // Encapsulated MSP payload
    outBuffer[5] = 0x30;                // header
    outBuffer[6] = packet->payloadSize; // mspPayloadSize
    outBuffer[7] = packet->function;    // packet->cmd

    // Copy packet payload into outBuffer
    for (uint16_t i = 0; i < packet->payloadSize; ++i)
    {
        outBuffer[8 + i] = packet->payload[i];
    }

    // Encapsulated MSP crc
    outBuffer[totalBufferLen - 2] = CalcCRCMsp(&outBuffer[6], packet->payloadSize + 2);

    // CRSF frame crc
    outBuffer[totalBufferLen - 1] = crsf_crc.calc(&outBuffer[2], packet->payloadSize + ENCAPSULATED_MSP_HEADER_CRC_LEN + CRSF_FRAME_LENGTH_EXT_TYPE_CRC - 1);
    processMessage(nullptr, (crsf_header_t *)outBuffer);
}
