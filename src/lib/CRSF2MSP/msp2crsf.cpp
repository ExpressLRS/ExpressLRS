#include "msp2crsf.h"
#include <iostream>

extern GENERIC_CRC8 crsf_crc;

MSP_COMMON MSP;

MSP2CROSSFIRE::MSP2CROSSFIRE() {} // empty constructor

void MSP2CROSSFIRE::setSeqNumber(uint8_t &data, uint8_t seqNumber)
{
    uint8_t seqNumberContrained = seqNumber & 0b1111; // constrain to bounds
    data &= ~(0b1111);                                // clear seq number
    data |= seqNumberContrained;                      // set seq number
}

void MSP2CROSSFIRE::setNewFrame(uint8_t &data, bool isNewFrame)
{
    if (isNewFrame)
    {
        data |= 0b10000; // set bit 4
    }
    else
    {
        data &= ~(0b10000); // clear bit 4
    }
}

void MSP2CROSSFIRE::setVersion(uint8_t &data, MSPframeType_e version)
{
    uint8_t val;

    if (version == MSP_FRAME_V1 || version == MSP_FRAME_V1_JUMBO)
    {
        val = 1;
    }
    else if (version == MSP_FRAME_V2)
    {
        val = 2;
    }
    else
    {
        setError(data, true);
        return;
    }
    data &= ~(0b11 << 5); // clear version
    data |= (val << 5);   // set version
}

uint8_t MSP2CROSSFIRE::getHeaderDir(uint8_t headerDir)
{
    if (headerDir == '<')
    {
        return 0x7A;
    }
    else if (headerDir == '>')
    {
        return 0x7B;
    }
    else
    {
        return 0;
    }
}

void MSP2CROSSFIRE::setError(uint8_t &data, bool isError)
{
    if (isError)
    {
        data |= 0b10000000; // set bit 7
    }
    else
    {
        data &= ~(0b10000000); // clear bit 7
    }
}

uint32_t MSP2CROSSFIRE::getFrameLen(uint32_t payloadLen, uint8_t mspVersion)
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

uint32_t MSP2CROSSFIRE::getPayloadLen(const uint8_t *data, MSPframeType_e mspVersion)
{
    uint32_t packetLen = 0;
    uint8_t lowByte;
    uint8_t highByte;

    switch (mspVersion)
    {
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
    }
    //std::cout << "Packet Length: " << (int)packetLen << std::endl;
    return packetLen;
}

MSPframeType_e MSP2CROSSFIRE::getVersion(const uint8_t *data)
{
    MSPframeType_e frameType;

    if (data[1] == 'M') // detect if V1 or V2
    {
        if (data[3] == 0xFF)
        {
            frameType = MSP_FRAME_V1_JUMBO;
        }
        else
        {
            frameType = MSP_FRAME_V1;
        }
    }
    else if (data[1] == 'X')
    {
        frameType = MSP_FRAME_V2;
    }
    else
    {
        frameType = MSP_FRAME_UNKNOWN;
    }
    return frameType;
}

void MSP2CROSSFIRE::parse(const uint8_t *data, uint32_t frameLen, uint8_t src, uint8_t dest)
{
    MSPframeType_e mspVersion = getVersion(data);
    uint32_t MSPpayloadLen = getPayloadLen(data, mspVersion);
    uint32_t MSPframeLen = getFrameLen(MSPpayloadLen, mspVersion);
                                                         // subtract 1 because crc/checksum is not included in encapsulated frame and we don't want to send it
    uint8_t numChunks = (MSPframeLen / CRSF_MSP_MAX_BYTES_PER_CHUNK) + 1; // gotta count the first chunk!
    uint8_t chunkRemainder = MSPframeLen % CRSF_MSP_MAX_BYTES_PER_CHUNK;

    uint8_t header[7];
    // first element has to be size of the fifo chunk (can't be bigger than CRSF_MAX_PACKET_LEN)
    header[0] = 0; // CRSFpktLen; will be set later
    header[1] = CRSF_ADDRESS_FLIGHT_CONTROLLER;
    header[2] = 0;                     // CRSFpktLen - CRSF_EXT_FRAME_PAYLOAD_LEN_SIZE_OFFSET;
    header[3] = getHeaderDir(data[2]); // 3rd byte is the header direction < or >
    header[4] = dest;
    header[5] = src;
    header[6] = 0;

    setVersion(header[6], mspVersion);

    // std::cout << "FIFO Size: " << (int)FIFOout.size() << std::endl;
    for (uint8_t i = 0; i < numChunks; i++)
    {
        // std::cout << "chunk: " << (int)i;
        setSeqNumber(header[6], i);
        setNewFrame(header[6], (i == 0 ? true : false)); // if first chunk then set to true, else false
        setError(header[6], false);

        uint32_t startIdx = (i * CRSF_MSP_MAX_BYTES_PER_CHUNK) + 3; // we don't xmit the MSP header
        uint8_t CRSFpktLen;                                         // TOTAL length of the CRSF packet, (what the FIFO cares about)

        if (i == (numChunks - 1)) // the last OR the only frame (chunk)
        {
            // std::cout << "PARTIAL CHUNK" << std::endl;
            CRSFpktLen = chunkRemainder + CRSF_EXT_FRAME_PAYLOAD_LEN_SIZE_OFFSET + 2; // status byte and CRC
            header[0] = CRSFpktLen;
            header[2] = CRSFpktLen - 2;
            FIFOout.pushBytes(header, sizeof(header));
            FIFOout.pushBytes(&data[startIdx], chunkRemainder);
            uint8_t crc = crsf_crc.calc(&header[3], sizeof(header) - 3, 0x00); // don't include the MSP header
            crc = crsf_crc.calc(&data[startIdx], chunkRemainder, crc);
            FIFOout.push(crc);
            // std::cout << "FIFO Size: " << (int)FIFOout.size() << std::endl;
        }
        else
        {
            // std::cout << "FULL CHUNK" << std::endl;
            CRSFpktLen = CRSF_MAX_PACKET_LEN;
            header[0] = CRSFpktLen;
            header[2] = CRSFpktLen - 2;
            FIFOout.pushBytes(header, sizeof(header));
            FIFOout.pushBytes(&data[startIdx], CRSF_MSP_MAX_BYTES_PER_CHUNK);
            uint8_t crc = crsf_crc.calc(&header[3], sizeof(header) - 3, 0x00); // don't include the MSP header
            crc = crsf_crc.calc(&data[startIdx], CRSF_MSP_MAX_BYTES_PER_CHUNK, crc);
            FIFOout.push(crc);
            // std::cout << "FIFO Size: " << (int)FIFOout.size() << std::endl;
        }
    }
}