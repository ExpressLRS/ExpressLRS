#include "msp2crsf.h"
#include <iostream>

extern GENERIC_CRC8 crsf_crc;

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

void MSP2CROSSFIRE::setVersion(uint8_t &data, uint8_t version)
{
    uint8_t val;

    if (version == 'M')
    {
        val = 1;
    }
    else if (version == 'X')
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

uint8_t MSP2CROSSFIRE::getV1payloadLen(const uint8_t *data)
{
    return data[3];
}

uint16_t MSP2CROSSFIRE::getV2payloadLen(const uint8_t *data)
{
    uint8_t lowByte = data[6];
    uint8_t highByte = data[7];
    return (highByte << 8) | lowByte;
}

void MSP2CROSSFIRE::parse(const uint8_t *data, uint32_t frameLen, uint8_t src, uint8_t dest)
{
    uint8_t MSPpayloadLen; // length of MSP payload
    uint8_t MSPbodyLen;    // length of MSP body (excludes header and CRC/Checksum)

    if (data[1] == 'M') // detect if V1 or V2
    {
        MSPpayloadLen = getV1payloadLen(data);
        MSPbodyLen = MSP_V1_BODY_LEN_FROM_PAYLOAD_LEN(MSPpayloadLen);
    }
    else if (data[1] == 'X')
    {
        MSPpayloadLen = getV2payloadLen(data);
        MSPbodyLen = MSP_V2_BODY_LEN_FROM_PAYLOAD_LEN(MSPpayloadLen);
    }
    else
    {
        DBGLN("MSP2CROSSFIRE::parse: invalid MSP version");
        return;
    }

    MSPbodyLen--;                                                        // subtract 1 because crc/checksum is not included in encapsulated frame and we don't want to send it
    uint8_t numChunks = (MSPbodyLen / CRSF_MSP_MAX_BYTES_PER_CHUNK) + 1; // gotta count the first chunk!
    uint8_t chunkRemainder = MSPbodyLen % CRSF_MSP_MAX_BYTES_PER_CHUNK;

    uint8_t header[7];
    // first element has to be size of the fifo chunk (can't be bigger than CRSF_MAX_PACKET_LEN)
    header[0] = 0; // CRSFpktLen; will be set later
    header[1] = CRSF_ADDRESS_FLIGHT_CONTROLLER;
    header[2] = 0;                     // CRSFpktLen - CRSF_EXT_FRAME_PAYLOAD_LEN_SIZE_OFFSET;
    header[3] = getHeaderDir(data[2]); // 3rd byte is the header direction < or >
    header[4] = dest;
    header[5] = src;
    header[6] = 0;

    setVersion(header[6], data[1]);

    //std::cout << "FIFO Size: " << (int)FIFOout.size() << std::endl;
    for (uint8_t i = 0; i < numChunks; i++)
    {
        //std::cout << "chunk: " << (int)i;
        setSeqNumber(header[6], i);
        setNewFrame(header[6], (i == 0 ? true : false)); // if first chunk then set to true, else false
        setError(header[6], false);

        uint32_t startIdx = (i * CRSF_MSP_MAX_BYTES_PER_CHUNK) + 3; // we don't xmit the MSP header
        uint8_t CRSFpktLen;                                         // TOTAL length of the CRSF packet, (what the FIFO cares about)

        if (i == (numChunks - 1)) // the last OR the only frame (chunk)
        {
            //std::cout << "PARTIAL CHUNK" << std::endl;
            CRSFpktLen = chunkRemainder + CRSF_EXT_FRAME_PAYLOAD_LEN_SIZE_OFFSET + 2; // status byte and CRC
            header[0] = CRSFpktLen;
            header[2] = CRSFpktLen - 2;
            FIFOout.pushBytes(header, sizeof(header));
            FIFOout.pushBytes(&data[startIdx], chunkRemainder);
            uint8_t crc = crsf_crc.calc(&header[3], sizeof(header) - 3, 0x00); // don't include the MSP header
            crc = crsf_crc.calc(&data[startIdx], chunkRemainder, crc);
            FIFOout.push(crc);
            //std::cout << "FIFO Size: " << (int)FIFOout.size() << std::endl;
        }
        else
        {
            //std::cout << "FULL CHUNK" << std::endl;
            CRSFpktLen = CRSF_MAX_PACKET_LEN;
            header[0] = CRSFpktLen;
            header[2] = CRSFpktLen - 2;
            FIFOout.pushBytes(header, sizeof(header));
            FIFOout.pushBytes(&data[startIdx], CRSF_MSP_MAX_BYTES_PER_CHUNK);
            uint8_t crc = crsf_crc.calc(&header[3], sizeof(header) - 3, 0x00); // don't include the MSP header
            crc = crsf_crc.calc(&data[startIdx], CRSF_MSP_MAX_BYTES_PER_CHUNK, crc);
            FIFOout.push(crc);
            //std::cout << "FIFO Size: " << (int)FIFOout.size() << std::endl;
        }
    }
}