#pragma once

#include <Arduino.h>

// TODO: MSP_PORT_INBUF_SIZE should be changed to
// dynamically allocate array length based on the payload size
// Hardcoding payload size to 8 bytes for now, since MSP is
// limited to a 4 byte payload on the BF side
#define MSP_PORT_INBUF_SIZE 8

#define CHECK_PACKET_PARSING() \
  if (packet->readError) {\
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
    mspPacketType_e type;
    uint8_t         flags;
    uint16_t        function;
    uint16_t        payloadSize;
    uint8_t         payload[MSP_PORT_INBUF_SIZE];
    uint16_t        payloadReadIterator;
    bool            readError;

    void reset()
    {
        type = MSP_PACKET_UNKNOWN;
        flags = 0;
        function = 0;
        payloadSize = 0;
        payloadReadIterator = 0;
        readError = false;
    }

    void addByte(uint8_t b)
    {
        payload[payloadSize++] = b;
    }

    void makeResponse()
    {
        type = MSP_PACKET_RESPONSE;
    }

    void makeCommand()
    {
        type = MSP_PACKET_COMMAND;
    }

    uint8_t readByte()
    {
        if (payloadReadIterator >= payloadSize) {
            // We are trying to read beyond the length of the payload
            readError = true;
            return 0;
        }

        return payload[payloadReadIterator++];
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
    uint8_t         convertToByteArray(mspPacket_t* packet, uint8_t* byteArray);
    uint8_t         getTotalPacketSize(mspPacket_t* packet);

private:
    mspState_e  m_inputState;
    uint16_t    m_offset;
    uint8_t     m_inputBuffer[MSP_PORT_INBUF_SIZE];
    mspPacket_t m_packet;
    uint8_t     m_crc;
};
