#include "crsf2msp.h"

#include "CRSFEndpoint.h"

CROSSFIRE2MSP::CROSSFIRE2MSP()
{
    reset();
}

void CROSSFIRE2MSP::reset()
{
    pktLen = 0;
    idx = 0;
    frameComplete = false;
    MSPvers = MSP_FRAME_UNKNOWN;
}

void CROSSFIRE2MSP::parse(const uint8_t *data)
{
    uint8_t CRSFpayloadLen = data[CRSF_FRAME_PAYLOAD_LEN_IDX] - CRSF_EXT_FRAME_PAYLOAD_LEN_SIZE_OFFSET;
    bool error = isError(data);
    bool newFrame = isNewFrame(data);

    bool seqError;
    uint8_t seqNumber = getSeqNumber(data);
    uint8_t SeqNumberNext;

    SeqNumberNext = (seqNumberPrev + 1) & 0b1111;
    SeqNumberNext == seqNumber ? seqError = false : seqError = true;
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
        outBuffer[2] = error ? '!' : getHeaderDir(data);
        pktLen = getFrameLen(data, MSPvers);
    }

    // process the chunk of MSP frame
    // if the last CRSF frame is zero padded we can't use the CRSF payload length
    // but if this isn't the last chunk we can't use the MSP payload length
    // the solution is to use the minimum of the two lengths
    uint32_t frameLen = pktLen - (idx - 3);
    uint32_t minLen = frameLen < CRSFpayloadLen ? frameLen : CRSFpayloadLen;
    memcpy(&outBuffer[idx], &data[CRSF_MSP_FRAME_OFFSET], minLen); // chunk of MSP data
    idx += minLen;

    if (idx - 3 == pktLen) // we have a complete MSP frame, -3 because the header isn't counted
    {
        // we need to append the MSP checksum
        outBuffer[idx] = getChecksum(outBuffer + 3, pktLen, MSPvers); // +3 because the header isn't in checksum
        frameComplete = true;

        FIFOout.lock();
        FIFOout.pushSize(idx + 1);
        FIFOout.pushBytes(outBuffer, idx + 1);
        FIFOout.unlock();
    }
}

bool CROSSFIRE2MSP::isNewFrame(const uint8_t *data)
{
    const uint8_t statusByte = data[CRSF_MSP_STATUS_BYTE_OFFSET];
    return (bool)((statusByte & 0b10000) >> 4); // bit active if there is a new frame
}

bool CROSSFIRE2MSP::isError(const uint8_t *data)
{
    const uint8_t statusByte = data[CRSF_MSP_STATUS_BYTE_OFFSET];
    return (bool)((statusByte & 0b10000000) >> 7);
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
    uint8_t headerVersion = ((statusByte & 0b01100000) >> 5);
    MSPframeType_e MSPvers;

    if (headerVersion == 1)
    {
        if (payloadLen == 0xFF)
        {
            MSPvers = MSP_FRAME_V1_JUMBO;
        }
        else
        {
            MSPvers = MSP_FRAME_V1;
        }
    }
    else if (headerVersion == 2)
    {
        MSPvers = MSP_FRAME_V2;
    }
    else
    {
        MSPvers = MSP_FRAME_UNKNOWN;
    }
    return MSPvers;
}

uint8_t CROSSFIRE2MSP::getChecksum(const uint8_t *data, const uint32_t len, MSPframeType_e mspVersion)
{
    uint8_t checkSum = 0;

    if (mspVersion == MSP_FRAME_V1 || mspVersion == MSP_FRAME_V1_JUMBO)
    {
        for (uint32_t i = 0; i < len; i++)
        {
            checkSum ^= data[i];
        }
        return checkSum;
    }
    else if (mspVersion == MSP_FRAME_V2)
    {
        checkSum = crsfEndpoint->crsf_crc.calc(data, len);
    }
    else
    {
        checkSum = 0;
    }
    return checkSum;
}

uint32_t CROSSFIRE2MSP::getFrameLen(const uint8_t *data, MSPframeType_e mspVersion)
{
    uint8_t lowByte;
    uint8_t highByte;

    if (mspVersion == MSP_FRAME_V1)
    {
        return (MSP_V1_FRAME_LEN_FROM_PAYLOAD_LEN(data[CRSF_MSP_FRAME_OFFSET]));
    }
    else if (mspVersion == MSP_FRAME_V1_JUMBO)
    {
        lowByte = data[CRSF_MSP_FRAME_OFFSET + 2];
        highByte = data[CRSF_MSP_FRAME_OFFSET + 3];
        return MSP_V1_JUMBO_FRAME_LEN_FROM_PAYLOAD_LEN((highByte << 8) | lowByte);
    }
    else if (mspVersion == MSP_FRAME_V2)
    {
        lowByte = data[CRSF_MSP_FRAME_OFFSET + 3];
        highByte = data[CRSF_MSP_FRAME_OFFSET + 4];
        return MSP_V2_FRAME_LEN_FROM_PAYLOAD_LEN((highByte << 8) | lowByte);
    }
    else
    {
        return 0;
    }
}

uint8_t CROSSFIRE2MSP::getHeaderDir(const uint8_t *data)
{
    const uint8_t statusByte = data[CRSF_MSP_TYPE_IDX];
    if (statusByte == 0x7A)
    {
        return '<';
    }
    else if (statusByte == 0x7B)
    {
        return '>';
    }
    else
    {
        return '!';
    }
}

bool CROSSFIRE2MSP::isFrameReady()
{
    return frameComplete;
}

const uint8_t *CROSSFIRE2MSP::getFrame()
{
    return outBuffer;
}

uint32_t CROSSFIRE2MSP::getFrameLen()
{
    return idx + 1; // include the last byte (crc)
}

uint8_t CROSSFIRE2MSP::getSrc()
{
    return src;
}

uint8_t CROSSFIRE2MSP::getDest()
{
    return dest;
}