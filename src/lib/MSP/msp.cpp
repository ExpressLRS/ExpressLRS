#include "msp.h"

/* ==========================================
MSP V2 Message Structure:
Offset: Usage:         In CRC:  Comment:
======= ======         =======  ========
0       $                       Framing magic start char
1       X                       'X' in place of v1 'M'
2       type                    '<' / '>' / '!' Message Type (TODO find out what ! type is)
3       flag           +        uint8, flag, usage to be defined (set to zero)
4       function       +        uint16 (little endian). 0 - 255 is the same function as V1 for backwards compatibility
6       payload size   +        uint16 (little endian) payload size in bytes
8       payload        +        n (up to 65535 bytes) payload
n+8     checksum                uint8, (n= payload size), crc8_dvb_s2 checksum
========================================== */

// CRC helper function. External to MSP class
// TODO: Move all our CRC functions to a CRC lib
uint8_t crc8_dvb_s2(uint8_t crc, unsigned char a)
{
    crc ^= a;
    for (int ii = 0; ii < 8; ++ii) {
        if (crc & 0x80) {
            crc = (crc << 1) ^ 0xD5;
        } else {
            crc = crc << 1;
        }
    }
    return crc;
}

bool
MSP::processReceivedByte(uint8_t c)
{
    switch (m_inputState) {

        case MSP_IDLE:
            // Wait for framing char
            if (c == '$') {
                m_inputState = MSP_HEADER_START;
            }
            break;

        case MSP_HEADER_START:
            // Waiting for 'X' (MSPv2 native)
            switch (c) {
                case 'X':
                    m_inputState = MSP_HEADER_X;
                    break;
                default:
                    m_inputState = MSP_IDLE;
                    break;
            }
            break;

        case MSP_HEADER_X:
            // Wait for the packet type (cmd or req)
            m_inputState = MSP_HEADER_V2_NATIVE;

            // Start of a new packet
            // reset the packet, offset iterator, and CRC
            m_packet.Reset();
            m_offset = 0;
            m_crc = 0;

            switch (c) {
                case '<':
                    m_packet.type = MSP_PACKET_COMMAND;
                    break;
                case '>':
                    m_packet.type = MSP_PACKET_REPLY;
                    break;
                default:
                    m_packet.type = MSP_PACKET_UNKNOWN;
                    m_inputState = MSP_IDLE;
                    break;
            }
            break;

        case MSP_HEADER_V2_NATIVE:
            // Read bytes until we have a full header
            m_inputBuffer[m_offset++] = c;
            m_crc = crc8_dvb_s2(m_crc, c);

            // If we've received the correct amount of bytes for a full header
            if (m_offset == sizeof(mspHeaderV2_t)) {
                // Copy header values into packet
                mspHeaderV2_t* header = (mspHeaderV2_t*)&m_inputBuffer[0];
                m_packet.payloadSize = header->payloadSize;
                m_packet.function = header->function;
                m_packet.flags = header->flags;
                // reset the offset iterator for re-use in payload below
                m_offset = 0;
            }
            break;

        case MSP_PAYLOAD_V2_NATIVE:
            // Read bytes until we reach payloadSize
            m_inputBuffer[m_offset++] = c;
            m_crc = crc8_dvb_s2(m_crc, c);

            // If we've received the correct amount of bytes for payload
            if (m_offset == m_packet.payloadSize) {
                // Then we're up to the CRC
                m_inputState = MSP_CHECKSUM_V2_NATIVE;
            }
            break;

        case MSP_CHECKSUM_V2_NATIVE:
            // Assert that the checksums match
            if (m_crc == c) {
                m_inputState = MSP_COMMAND_RECEIVED;
            }
            else {
                m_inputState = MSP_IDLE;
            }
            break;
        
        default:
            m_inputState = MSP_IDLE;
            break;
    }

    if (m_inputState == MSP_COMMAND_RECEIVED) {
        return true;
    }
    return false;
}

mspPacket_t
MSP::getReceivedPacket() const
{
    return m_packet;
}

void
MSP::markPacketReceived()
{
    m_inputState = MSP_IDLE;
}
