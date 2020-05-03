#pragma once

#include "platform.h"
#include <Arduino.h>

// TODO: MSP_PORT_INBUF_SIZE should be changed to
// dynamically allocate array length based on the payload size
// Hardcoding payload size to 8 bytes for now, since MSP is
// limited to a 4 byte payload on the BF side
#define MSP_PORT_INBUF_SIZE 8

#define CHECK_PACKET_PARSING() \
  if (packet->error) {\
    return;\
  }

typedef enum {
    MSP_IDLE,
    MSP_HEADER_START,
    MSP_HEADER_X,

    MSP_HEADER_V2_NATIVE,
    MSP_PAYLOAD_V2_NATIVE,
    MSP_CHECKSUM_V2_NATIVE,

    MSP_COMMAND_RECEIVED
} mspState_e;

typedef enum {
    MSP_PACKET_UNKNOWN,
    MSP_PACKET_COMMAND,
    MSP_PACKET_RESPONSE
} mspPacketType_e;

typedef struct __attribute__((packed)) {
    uint8_t  flags;
    uint16_t function;
    uint16_t payloadSize;
} mspHeaderV2_t;

typedef struct {
    mspPacketType_e volatile type;
    uint8_t         volatile flags;
    uint16_t        volatile function;
    uint16_t        volatile payloadSize;
    uint8_t         volatile payload[MSP_PORT_INBUF_SIZE];
    uint16_t        volatile payloadIterator;
    bool            volatile error;

    uint8_t ICACHE_RAM_ATTR iterated()
    {
        return (payloadSize <= payloadIterator);
    }

    void ICACHE_RAM_ATTR reset()
    {
        type = MSP_PACKET_UNKNOWN;
        flags = 0;
        function = 0;
        payloadSize = 0;
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

    void ICACHE_RAM_ATTR setIteratorToSize()
    {
        payloadSize = payloadIterator;
        payloadIterator = 0;
    }

    void ICACHE_RAM_ATTR makeResponse()
    {
        type = MSP_PACKET_RESPONSE;
    }

    void ICACHE_RAM_ATTR makeCommand()
    {
        type = MSP_PACKET_COMMAND;
    }

    uint8_t ICACHE_RAM_ATTR readByte()
    {
        if (iterated()) {
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
    bool            processReceivedByte(uint8_t c);
    mspPacket_t*    getReceivedPacket();
    void            markPacketReceived();
    bool            sendPacket(mspPacket_t* packet, Stream* port);

private:
    mspState_e  m_inputState;
    uint16_t    m_offset;
    uint8_t     m_inputBuffer[MSP_PORT_INBUF_SIZE];
    mspPacket_t m_packet;
    uint8_t     m_crc;
};
