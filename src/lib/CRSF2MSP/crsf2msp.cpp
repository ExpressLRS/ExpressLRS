#include "crsf2msp.h"
//#include <iostream> // for test

extern GENERIC_CRC8 crsf_crc; // defined in crsf.cpp reused here

CROSSFIRE2MSP::CROSSFIRE2MSP()
{
    // clean inital state
    memset(outBuffer, 0x1, MSP_FRAME_MAX_LEN);
    pktLen = 0;
    idx = 0;
    frameComplete = false;
}

void CROSSFIRE2MSP::parse(const uint8_t *data, uint8_t len)
{
    uint8_t statusByte = data[CRSF_MSP_STATUS_BYTE_OFFSET]; // status byte from CRSF MSP frame

    bool error = isError(statusByte);
    bool newFrame = isNewFrame(statusByte);
    uint8_t seqNum = getSeqNumber(statusByte);
    uint8_t MSPvers = getVersion(statusByte);

    if (error)
    {
        pktLen = 0;
        idx = 0;
        frameComplete = false;
        memset(outBuffer, 0, sizeof(outBuffer));
        // DBGLN("MSP frame error bit set!");
        return;
    }

    uint8_t CRSFpayloadLen = data[CRSF_FRAME_PAYLOAD_LEN_IDX] - CRSF_EXT_FRAME_PAYLOAD_LEN_SIZE_OFFSET + 1;
    //std::cout << "CRSFpayloadLen: " << std::hex << (int)CRSFpayloadLen << std::endl;

    if (newFrame) // single packet or first chunk of series of packets
    {
        memset(outBuffer, 0, sizeof(outBuffer));
        src = data[CRSF_MSP_SRC_OFFSET];
        dest = data[CRSF_MSP_DEST_OFFSET];
        uint8_t header[3];
        header[0] = '$';
        header[1] = MSPvers == 1 ? 'M' : 'X';
        header[2] = getHeaderDir(data[CRSF_MSP_TYPE_IDX]);
        memcpy(&outBuffer[0], header, sizeof(header));
        pktLen = getFrameLen(data, MSPvers) -1; // -1 for checksum which is not needed
        idx = 3;                                 // offset by 3 bytes for MSP header

        if (pktLen > CRSF_MSP_MAX_BYTES_PER_CHUNK)
        { // we end up here if we have a chunked message
            memcpy(&outBuffer[idx], &data[CRSF_MSP_FRAME_OFFSET], CRSFpayloadLen - 1);
            idx += CRSFpayloadLen - 1;
        }
        else
        { // fits in a single CRSF frame
            memcpy(&outBuffer[idx], &data[CRSF_MSP_FRAME_OFFSET], pktLen);
            idx += pktLen;
        }
    }
    else
    { // process the next chunk of MSP frame
        // if the last CRSF frame is zero padded we can't use the CRSF payload length
        // but if this isn't the last chunk we can't use the MSP payload length
        // the solution is to use the minimum of the two lengths
        uint32_t a = (uint32_t)CRSFpayloadLen;
        uint32_t b = pktLen - (idx - 3);
        uint8_t minLen = !(b < a) ? a : b;
        memcpy(&outBuffer[idx], &data[CRSF_MSP_FRAME_OFFSET], minLen); // next chunk of data
        idx += minLen;
    }

    if ((idx - 3) == pktLen) // we have a complete MSP frame, -3 because the header isn't counted
    {
        frameComplete = true;
        // we need to overwrite the CRSF checksum with the MSP checksum
        uint8_t crc = getChecksum(outBuffer, MSPvers);
        outBuffer[idx] = crc;
    }
}

bool CROSSFIRE2MSP::isNewFrame(uint8_t data)
{
    return (bool)((data & 0b10000) >> 4); // bit active if there is a new frame
}

bool CROSSFIRE2MSP::isError(uint8_t data)
{
    return (bool)((data & 0b10000000) >> 7);
}

uint8_t CROSSFIRE2MSP::getSeqNumber(uint8_t data)
{
    return data & 0b1111; // first four bits is seq number
}

uint8_t CROSSFIRE2MSP::getChecksum(const uint8_t *data, uint8_t mspVersion)
{
    const uint8_t startIdx = 3; // skip the $M</> header  in both cases
    uint8_t checkSum = 0;

    if (mspVersion == 1)
    {
        uint8_t len = data[3] + 2;  // payload len plus function
        for (uint8_t i = 0; i < len; i++)
        {
            checkSum ^= data[i + startIdx];
        }
        return checkSum;
    }
    else if (mspVersion == 2)
    {
        uint8_t lowByte = data[6];
        uint8_t highByte = data[7];
        uint32_t len = (highByte << 8) | lowByte;
        uint32_t crcLen = len + 5; // +5 uin8_t flags, uint16 function, uint16 payloadLen
        checkSum = crsf_crc.calc(&data[startIdx], crcLen, 0x00);
    }
    else
    {
        checkSum = 0;
    }
    return checkSum;
}

uint32_t CROSSFIRE2MSP::getFrameLen(const uint8_t *data, uint8_t mspVersion)
{
    if (mspVersion == 1)
    {
        return MSP_V1_BODY_LEN_FROM_PAYLOAD_LEN(data[CRSF_MSP_FRAME_OFFSET]);
    }
    else if (mspVersion == 2)
    {
        uint8_t lowByte = data[CRSF_MSP_FRAME_OFFSET + 3];
        uint8_t highByte = data[CRSF_MSP_FRAME_OFFSET + 4];
        return MSP_V2_BODY_LEN_FROM_PAYLOAD_LEN((highByte << 8) | lowByte);
    }
    else
    {
        return 0;
    }
}

uint8_t CROSSFIRE2MSP::getVersion(uint8_t data)
{
    return ((data & 0b01100000) >> 5);
}

uint8_t CROSSFIRE2MSP::getHeaderDir(uint8_t data)
{
    if (data == 0x7A)
    {
        return '<';
    }
    else if (data == 0x7B)
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
    return idx + 1;
}

uint8_t CROSSFIRE2MSP::getSrc()
{
    return src;
}

uint8_t CROSSFIRE2MSP::getDest()
{
    return dest;
}