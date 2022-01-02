#include "crsf2msp.h"
#include <iostream> // for test

extern GENERIC_CRC8 crsf_crc; // defined in crsf.cpp reused here

CROSSFIRE2MSP::CROSSFIRE2MSP()
{
    reset();
}

void CROSSFIRE2MSP::reset()
{
    // clean state
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
    seqNumber = getSeqNumber(data);
    uint8_t SeqNumberNext;

    SeqNumberNext = (seqNumberPrev + 1) & 0b1111;
    SeqNumberNext == seqNumber ? seqError = false : seqError = true;
    seqNumberPrev = seqNumber;

    if ((!newFrame && seqError) || error)
    {
        DBGLN("Seq Error! Len: %d Sgot: %d Sxpt: %d", pktLen, seqNumber, SeqNumberNext);
        reset();
        return;
    }

    if (newFrame) // single packet or first chunk of series of packets
    {
        idx = 3; // skip the header start wiring at offset 3.
        MSPvers = getVersion(data);
        src = data[CRSF_MSP_SRC_OFFSET];
        dest = data[CRSF_MSP_DEST_OFFSET];
        uint8_t header[3];
        header[0] = '$';
        header[1] = (MSPvers == MSP_FRAME_V1 || MSPvers == MSP_FRAME_V1_JUMBO) ? 'M' : 'X';
        header[2] = error ? '!' : getHeaderDir(data);
        memcpy(&outBuffer[0], header, sizeof(header));
        pktLen = getFrameLen(data, MSPvers);

        memcpy(&outBuffer[idx], &data[CRSF_MSP_FRAME_OFFSET], CRSFpayloadLen);
        idx += CRSFpayloadLen;
    }
    else
    { // process the next chunk of MSP frame
        // if the last CRSF frame is zero padded we can't use the CRSF payload length
        // but if this isn't the last chunk we can't use the MSP payload length
        // the solution is to use the minimum of the two lengths
        uint32_t a = (uint32_t)CRSFpayloadLen;
        uint32_t b = pktLen - (idx - 3);
        uint32_t minLen = !(b < a) ? a : b;
        memcpy(&outBuffer[idx], &data[CRSF_MSP_FRAME_OFFSET], minLen); // next chunk of data
        idx += minLen;
    }

    const uint16_t idxSansHeader = idx - 3;
    if (idxSansHeader >= pktLen) // we have a complete MSP frame, -3 because the header isn't counted
    {
        // we need to overwrite the CRSF checksum with the MSP checksum
        uint8_t crc = getChecksum(outBuffer, pktLen, MSPvers);
        outBuffer[idx] = crc;
        frameComplete = true;

        FIFOout.pushSize(idx + 1);
        FIFOout.pushBytes(outBuffer, idx + 1);
    }
    else if (idxSansHeader > pktLen)
    {
        DBGLN("Got too much data! Len: %d got: %d", pktLen, idxSansHeader);
        reset();
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
    const uint8_t startIdx = 3; // skip the $M</> header  in both cases
    uint8_t checkSum = 0;

    if (mspVersion == MSP_FRAME_V1 || mspVersion == MSP_FRAME_V1_JUMBO)
    {
        for (uint32_t i = 0; i < len; i++)
        {
            checkSum ^= data[i + startIdx];
        }
        return checkSum;
    }
    else if (mspVersion == MSP_FRAME_V2)
    {
        checkSum = crsf_crc.calc(&data[startIdx], len);
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