#include "crsf2msp.h"

#include "CRSFRouter.h"

void CROSSFIRE2MSP::reset()
{
    pktLen = 0;
    idx = 0;
    frameComplete = false;
    MSPvers = MSP_FRAME_UNKNOWN;
}

void CROSSFIRE2MSP::parse(const uint8_t *data, const std::function<void(uint8_t *, uint32_t)> &processMSP)
{
    const uint8_t CRSFpayloadLen = data[CRSF_FRAME_PAYLOAD_LEN_IDX] - CRSF_EXT_FRAME_PAYLOAD_LEN_SIZE_OFFSET;
    const bool error = isError(data);
    const bool newFrame = isNewFrame(data);
    const uint8_t seqNumber = getSeqNumber(data);
    const uint8_t SeqNumberNext = (seqNumberPrev + 1) & 0b1111;
    const bool seqError = SeqNumberNext != seqNumber;
    seqNumberPrev = seqNumber;

    if ((!newFrame && seqError) || error)
    {
        reset();
        return;
    }

    if (newFrame) // If it's a new frame then out a header on first
    {
        idx = 3; // skip the header start wiring at offset 3.
        MSPvers = getVersion(data);
        src = data[CRSF_MSP_SRC_OFFSET];
        dest = data[CRSF_MSP_DEST_OFFSET];
        outBuffer[0] = '$';
        outBuffer[1] = (MSPvers == MSP_FRAME_V1 || MSPvers == MSP_FRAME_V1_JUMBO) ? 'M' : 'X';
        outBuffer[2] = getHeaderDir(data);
        pktLen = getFrameLen(data, MSPvers);
    }

    // process the chunk of MSP frame
    // if the last CRSF frame is zero padded we can't use the CRSF payload length
    // but if this isn't the last chunk we can't use the MSP payload length
    // the solution is to use the minimum of the two lengths
    const uint32_t frameLen = pktLen - (idx - 3);
    const uint32_t minLen = frameLen < CRSFpayloadLen ? frameLen : CRSFpayloadLen;
    memcpy(&outBuffer[idx], &data[CRSF_MSP_FRAME_OFFSET], minLen); // chunk of MSP data
    idx += minLen;

    if (idx - 3 == pktLen) // we have a complete MSP frame, -3 because the header isn't counted
    {
        // we need to append the MSP checksum
        outBuffer[idx] = getChecksum(outBuffer + 3, pktLen, MSPvers); // +3 because the header isn't in checksum
        frameComplete = true;
        processMSP(outBuffer, idx + 1);
    }
}

bool CROSSFIRE2MSP::isNewFrame(const uint8_t *data)
{
    const uint8_t statusByte = data[CRSF_MSP_STATUS_BYTE_OFFSET];
    return statusByte & bit(4); // bit active if there is a new frame
}

bool CROSSFIRE2MSP::isError(const uint8_t *data)
{
    const uint8_t statusByte = data[CRSF_MSP_STATUS_BYTE_OFFSET];
    return statusByte & bit(7); // bit active if there is an error
}

uint8_t CROSSFIRE2MSP::getSeqNumber(const uint8_t *data)
{
    const uint8_t statusByte = data[CRSF_MSP_STATUS_BYTE_OFFSET];
    return statusByte & 0b1111; // first four bits is seq number
}

MSPframeType_e CROSSFIRE2MSP::getVersion(const uint8_t *data)
{
    const uint8_t statusByte = data[CRSF_MSP_STATUS_BYTE_OFFSET];
    const uint8_t payloadLen = data[CRSF_MSP_FRAME_OFFSET]; // first element is the payload length
    const uint8_t headerVersion = ((statusByte & 0b01100000) >> 5);

    if (headerVersion == 1)
    {
        if (payloadLen == 0xFF)
        {
            return MSP_FRAME_V1_JUMBO;
        }
        return MSP_FRAME_V1;
    }
    if (headerVersion == 2)
    {
        return MSP_FRAME_V2;
    }
    return MSP_FRAME_UNKNOWN;
}

uint8_t CROSSFIRE2MSP::getChecksum(const uint8_t *data, const uint32_t packetLen, const MSPframeType_e mspVersion)
{
    if (mspVersion == MSP_FRAME_V1 || mspVersion == MSP_FRAME_V1_JUMBO)
    {
        uint8_t checkSum = 0;
        for (uint32_t i = 0; i < packetLen; i++)
        {
            checkSum ^= data[i];
        }
        return checkSum;
    }
    if (mspVersion == MSP_FRAME_V2)
    {
        return crsfRouter.crsf_crc.calc(data, packetLen);
    }
    return 0;
}

uint32_t CROSSFIRE2MSP::getFrameLen(const uint8_t *data, const MSPframeType_e mspVersion)
{
    uint8_t lowByte;
    uint8_t highByte;

    if (mspVersion == MSP_FRAME_V1)
    {
        return MSP_V1_FRAME_LEN_FROM_PAYLOAD_LEN(data[CRSF_MSP_FRAME_OFFSET]);
    }
    if (mspVersion == MSP_FRAME_V1_JUMBO)
    {
        lowByte = data[CRSF_MSP_FRAME_OFFSET + 2];
        highByte = data[CRSF_MSP_FRAME_OFFSET + 3];
        return MSP_V1_JUMBO_FRAME_LEN_FROM_PAYLOAD_LEN((highByte << 8) | lowByte);
    }
    if (mspVersion == MSP_FRAME_V2)
    {
        lowByte = data[CRSF_MSP_FRAME_OFFSET + 3];
        highByte = data[CRSF_MSP_FRAME_OFFSET + 4];
        return MSP_V2_FRAME_LEN_FROM_PAYLOAD_LEN((highByte << 8) | lowByte);
    }
    return 0;
}

uint8_t CROSSFIRE2MSP::getHeaderDir(const uint8_t *data)
{
    const uint8_t statusByte = data[CRSF_MSP_TYPE_IDX];
    if (statusByte == CRSF_FRAMETYPE_MSP_REQ)
    {
        return '<';
    }
    if (statusByte == CRSF_FRAMETYPE_MSP_RESP)
    {
        return '>';
    }
    return '!';
}
