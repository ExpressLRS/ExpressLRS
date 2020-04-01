#pragma once

#include <Arduino.h>

// TODO: MSP_PORT_INBUF_SIZE could be changed to dynamically allocate array length based on the payload size
#define MSP_PORT_INBUF_SIZE 192

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
    MSP_PACKET_REPLY
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

    void Reset()
    {
        type = MSP_PACKET_UNKNOWN;
        flags = 0;
        function = 0;
        payloadSize = 0;
    }
} mspPacket_t;

/////////////////////////////////////////////////

class MSP
{
public:
    bool        processReceivedByte(uint8_t c);
    mspPacket_t getReceivedPacket() const;
    void        markPacketReceived();

private:
    mspState_e  m_inputState;
    uint16_t    m_offset;
    uint8_t     m_inputBuffer[MSP_PORT_INBUF_SIZE];
    mspPacket_t m_packet;
    uint8_t     m_crc;
};
