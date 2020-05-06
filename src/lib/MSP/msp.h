#pragma once

#include "platform.h"
#include "helpers.h"
#include <HardwareSerial.h>
#include <Arduino.h>

// TODO: MSP_PORT_INBUF_SIZE should be changed to
// dynamically allocate array length based on the payload size
// Hardcoding payload size to 8 bytes for now, since MSP is
// limited to a 4 byte payload on the BF side
//#define MSP_PORT_INBUF_SIZE 16 // was 8
#define MSP_PORT_INBUF_SIZE 256

#define CHECK_PACKET_PARSING() \
    if (packet->error)         \
    {                          \
        return;                \
    }

#define MSP_VERSION (1U << 5)
#define MSP_STARTFLAG (1U << 4)

typedef enum
{
    MSP_IDLE,
    MSP_MSP_START,
    MSP_HEADER_M, // MSPv1
    MSP_HEADER_X, // MSPv2

    MSP_PAYLOAD_SIZE,
    MSP_PAYLOAD_FUNC,
    MSP_PAYLOAD,
    MSP_CHECKSUM,

    MSP_HEADER_V2_NATIVE,
    MSP_PAYLOAD_V2_NATIVE,
    MSP_CHECKSUM_V2_NATIVE,

    MSP_COMMAND_RECEIVED
} mspState_e;

typedef enum
{
    MSP_PACKET_UNKNOWN,
    MSP_PACKET_V1_CMD,
    MSP_PACKET_V1_RESP,
    MSP_PACKET_COMMAND,
    MSP_PACKET_RESPONSE
} mspPacketType_e;

enum
{
    MSP_VTX_ONFIG = 0x58,      // read
    MSP_VTX_SET_CONFIG = 0x59, // write
};

typedef struct PACKED
{
    uint8_t flags;
    uint8_t payloadSize;
    uint8_t function;
    uint8_t payload[];
} mspHeaderV1_t;

typedef struct PACKED
{
    uint8_t flags;
    uint16_t function;
    uint16_t payloadSize;
} mspHeaderV2_t;

typedef struct
{
    mspPacketType_e volatile type;
    uint8_t volatile WORD_ALIGNED_ATTR payload[MSP_PORT_INBUF_SIZE];
    uint16_t volatile function;
    uint16_t volatile payloadSize;
    uint16_t volatile payloadIterator;
    uint8_t volatile flags;
    bool volatile error;

    inline uint8_t iterated()
    {
        return (0 < payloadSize && payloadSize <= payloadIterator);
    }

    inline void ICACHE_RAM_ATTR reset()
    {
        type = MSP_PACKET_UNKNOWN;
        flags = 0;
        function = 0;
        payloadSize = 0;
        payloadIterator = 0;
        error = false;
    }
    void ICACHE_RAM_ATTR reset(mspHeaderV1_t *hdr)
    {
        type = MSP_PACKET_UNKNOWN;
        flags = hdr->flags;
        function = hdr->function;
        payloadSize = hdr->payloadSize;
        payloadIterator = 0;
        error = false;
    }
    void ICACHE_RAM_ATTR reset(mspHeaderV2_t *hdr)
    {
        type = MSP_PACKET_UNKNOWN;
        flags = hdr->flags;
        function = hdr->function;
        payloadSize = hdr->payloadSize;
        payloadIterator = 0;
        error = false;
    }

    void ICACHE_RAM_ATTR addByte(uint8_t b)
    {
        if (payloadIterator >= sizeof(payload))
        {
            error = true;
            return;
        }
        payload[payloadIterator++] = b;
    }

    inline void ICACHE_RAM_ATTR setIteratorToSize()
    {
        payloadSize = payloadIterator;
        payloadIterator = 0;
    }

    inline void ICACHE_RAM_ATTR makeResponse()
    {
        type = MSP_PACKET_RESPONSE;
    }

    inline void ICACHE_RAM_ATTR makeCommand()
    {
        type = MSP_PACKET_COMMAND;
    }

    uint8_t ICACHE_RAM_ATTR readByte()
    {
        if (iterated())
        {
            // We are trying to read beyond the length of the payload
            error = true;
            return 0;
        }
        return payload[payloadIterator++];
    }
} mspPacket_t;

/////////////////////////////////////////////////

class MSP
{
public:
    bool processReceivedByte(uint8_t c);
    mspPacket_t *getReceivedPacket();
    void markPacketReceived();
    bool sendPacket(mspPacket_t *packet, HardwareSerial *port);

private:
    mspPacket_t m_packet;
    mspState_e m_inputState = MSP_IDLE;
    uint16_t m_offset = 0;
    uint8_t WORD_ALIGNED_ATTR m_inputBuffer[MSP_PORT_INBUF_SIZE];
    uint8_t m_crc = 0, m_crc_v1 = 0;
};
