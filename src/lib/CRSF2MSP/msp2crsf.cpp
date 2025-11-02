#include "msp2crsf.h"

#include "CRSFRouter.h"

uint8_t MSP2CROSSFIRE::getSeqNumberBits(const uint8_t seqNumber)
{
    return seqNumber & 0b1111; // constrain to bounds
}

uint8_t MSP2CROSSFIRE::getNewFrameBits(bool isNewFrame)
{
    return isNewFrame ? bit(4) : 0;
}

uint8_t MSP2CROSSFIRE::getVersionBits(const MSPframeType_e version)
{
    if (version == MSP_FRAME_V1 || version == MSP_FRAME_V1_JUMBO)
    {
        return 1 << 5;
    }
    if (version == MSP_FRAME_V2)
    {
        return 2 << 5;
    }
    return bit(7);  // Set the error bit for invalid MSP versions
}

crsf_frame_type_e MSP2CROSSFIRE::getHeaderDir(const uint8_t headerDir)
{
    if (headerDir == '<')
    {
        return CRSF_FRAMETYPE_MSP_REQ;
    }
    if (headerDir == '>')
    {
        return CRSF_FRAMETYPE_MSP_RESP;
    }
    return (crsf_frame_type_e)0;
}

uint32_t MSP2CROSSFIRE::getFrameLen(const uint32_t payloadLen, const uint8_t mspVersion)
{
    uint32_t frameLen = 0;
    switch (mspVersion)
    {
        {
        case MSP_FRAME_V1:
            frameLen = MSP_V1_FRAME_LEN_FROM_PAYLOAD_LEN(payloadLen);
            break;

        case MSP_FRAME_V2:
            frameLen = MSP_V2_FRAME_LEN_FROM_PAYLOAD_LEN(payloadLen);
            break;

        case MSP_FRAME_V1_JUMBO:
            frameLen = MSP_V1_JUMBO_FRAME_LEN_FROM_PAYLOAD_LEN(payloadLen);
            break;

        default:
            break;
        }
    }
    return frameLen;
}

uint32_t MSP2CROSSFIRE::getPayloadLen(const uint8_t *data, const MSPframeType_e mspVersion)
{
    uint32_t packetLen = 0;
    uint8_t lowByte;
    uint8_t highByte;

    switch (mspVersion)
    {
    case MSP_FRAME_V1:
        packetLen = data[3];
        break;

    case MSP_FRAME_V2:
        lowByte = data[6];
        highByte = data[7];
        packetLen = (highByte << 8) | lowByte;
        break;

    case MSP_FRAME_V1_JUMBO:
        lowByte = data[5];
        highByte = data[6];
        packetLen = (highByte << 8) | lowByte;
        break;

    default:
        break;
    }
    return packetLen;
}

MSPframeType_e MSP2CROSSFIRE::getVersion(const uint8_t *data)
{
    if (data[1] == 'M') // detect if V1 or V2
    {
        if (data[3] == 0xFF)
        {
            return MSP_FRAME_V1_JUMBO;
        }
        return MSP_FRAME_V1;
    }
    if (data[1] == 'X')
    {
        return MSP_FRAME_V2;
    }
    return MSP_FRAME_UNKNOWN;
}

void MSP2CROSSFIRE::parse(CRSFConnector *connector, const uint8_t *data, uint32_t frameLen, const crsf_addr_e src, const crsf_addr_e dest)
{
    const MSPframeType_e mspVersion = getVersion(data);
    const uint32_t mspPayloadLen = getPayloadLen(data, mspVersion);
    const uint32_t MSPframeLen = getFrameLen(mspPayloadLen, mspVersion);
    const uint8_t numChunks = (MSPframeLen / CRSF_MSP_MAX_BYTES_PER_CHUNK) + 1; // count the first chunk!
    const uint8_t chunkRemainder = MSPframeLen % CRSF_MSP_MAX_BYTES_PER_CHUNK;

    for (uint8_t i = 0; i < numChunks; i++)
    {
        uint8_t packet[CRSF_MAX_PACKET_LEN] {};
        packet[5] = getVersionBits(mspVersion) | getSeqNumberBits(seqNum++) | getNewFrameBits(i == 0);

        const uint32_t startIdx = (i * CRSF_MSP_MAX_BYTES_PER_CHUNK) + 3; // we don't transmit the MSP header
        const uint8_t CRSFpktLen = (i == (numChunks - 1)) ? chunkRemainder : (CRSF_MSP_MAX_BYTES_PER_CHUNK);

        memcpy(&packet[6], &data[startIdx], CRSFpktLen);
        crsfRouter.SetExtendedHeaderAndCrc((crsf_ext_header_t *)packet, getHeaderDir(data[CRSF_MSP_TYPE_IDX]), CRSFpktLen + CRSF_EXT_FRAME_PAYLOAD_LEN_SIZE_OFFSET, dest, src);
        crsfRouter.processMessage(connector, (crsf_header_t *)packet);
    }
}

bool MSP2CROSSFIRE::validate(const uint8_t *data, const uint32_t expectLen)
{
    const MSPframeType_e version = getVersion(data);
    const uint32_t FrameLen = getFrameLen(getPayloadLen(data, version), version) + 4; // +3 header, +1 crc;
    return expectLen == FrameLen;
}