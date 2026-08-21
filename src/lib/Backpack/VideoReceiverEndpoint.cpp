#include "VideoReceiverEndpoint.h"

#if !defined(UNIT_TEST)
#include "config.h"

extern Stream *BackpackOrLogStrm;
#endif

// Encapsulated MSP status byte, at the start of the CRSF extended payload
#define MSP_STATUS_ERROR        0x80
#define MSP_STATUS_START        0x10
#define MSP_STATUS_VERSION(s)   (((s) >> 5) & 0x03)

#define MSP_V1_HEADER_LEN       3   // status, size, function
#define MSP_V2_HEADER_LEN       6   // status, flags, function(2), size(2)

VideoReceiverEndpoint videoReceiver;

bool decodeEncapsulatedMsp(const crsf_header_t *message, mspPacket_t *packet)
{
    if (message->type != CRSF_FRAMETYPE_MSP_WRITE && message->type != CRSF_FRAMETYPE_MSP_REQ)
    {
        return false;
    }

    const auto extMessage = (const crsf_ext_header_t *)message;
    const uint8_t status = extMessage->payload[0];

    // Reject errored frames, and continuations of chunked messages which we
    // don't reassemble; everything we relay today fits in a single frame.
    if ((status & MSP_STATUS_ERROR) || !(status & MSP_STATUS_START))
    {
        return false;
    }

    uint8_t headerLen;
    uint8_t flags;
    uint16_t function;
    uint16_t payloadSize;

    switch (MSP_STATUS_VERSION(status))
    {
        case 1:
            headerLen = MSP_V1_HEADER_LEN;
            flags = 0;
            payloadSize = extMessage->payload[1];
            function = extMessage->payload[2];
            break;
        case 2:
            headerLen = MSP_V2_HEADER_LEN;
            flags = extMessage->payload[1];
            function = extMessage->payload[2] | (extMessage->payload[3] << 8);
            payloadSize = extMessage->payload[4] | (extMessage->payload[5] << 8);
            break;
        default:
            return false;
    }

    // Encapsulated MSP bytes carried by this frame, excluding the trailing MSP CRC
    const int available = message->frame_size - CRSF_FRAME_LENGTH_EXT_TYPE_CRC - 1;
    if (payloadSize > MSP_PORT_INBUF_SIZE || (int)(headerLen + payloadSize) > available)
    {
        return false;
    }

    packet->reset();
    packet->makeCommand();
    packet->flags = flags;
    packet->function = function;
    for (uint16_t i = 0; i < payloadSize; i++)
    {
        packet->addByte(extMessage->payload[headerLen + i]);
    }
    return true;
}

void VideoReceiverEndpoint::handleMessage(const crsf_header_t *message)
{
#if !defined(UNIT_TEST)
    if (config.GetBackpackDisable())
    {
        return;
    }

    mspPacket_t packet;
    if (decodeEncapsulatedMsp(message, &packet))
    {
        MSP::sendPacket(&packet, BackpackOrLogStrm);
    }
#endif
}
