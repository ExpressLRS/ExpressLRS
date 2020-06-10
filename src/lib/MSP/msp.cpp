#include "msp.h"
#include "crc.h"
#include "platform.h"
#include "debug_elrs.h"

/* ==========================================
MSP V1 Message Structure:
Offset: Usage:         In CRC:  Comment:
======= ======         =======  ========
0       $                       Framing magic start char
1       M                       MSP version 1
2       type                    MSP direction / status flag: '<' / '>' / '!' (error)
3       payload size   +        uint8 payload size in bytes
4       frame id       +        uint8 MSPv1 frame ID
5       payload        +        n (up to 254 bytes) payload
n+5     checksum                uint8, (n= payload size), XOR checksum

==========================================

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

bool MSP::processReceivedByte(uint8_t c)
{
    switch (m_inputState)
    {
        case MSP_IDLE:
            // Wait for framing char
            if (c == '$')
            {
                m_inputState = MSP_MSP_START;
            }
            break;

        case MSP_MSP_START:
            switch (c)
            {
                case 'X':
                    // Waiting for 'X' (MSPv2 native)
                    m_inputState = MSP_HEADER_X;
                    break;
                case 'M':
                    // Waiting for 'M' (MSPv1 native)
                    m_inputState = MSP_HEADER_M;
                    break;
                default:
                    m_inputState = MSP_IDLE;
                    break;
            }
            break;

        /****************** MSPv1 PARSING **********************/
        case MSP_HEADER_M:
        {
            /* MSP direction or status flag */
            m_inputState = MSP_PAYLOAD_SIZE;
            m_packet.reset();
            m_offset = 0;
            m_crc = m_crc_v1 = 0;

            switch (c)
            {
                case '?':
                    m_packet.type = MSP_PACKET_V1_ELRS;
                    m_inputState = MSP_FLAGS;
                    break;
                case '<':
                    m_packet.type = MSP_PACKET_V1_CMD;
                    break;
                case '>':
                    m_packet.type = MSP_PACKET_V1_RESP;
                    break;
                default:
                    m_packet.type = MSP_PACKET_UNKNOWN;
                    m_inputState = MSP_IDLE;
                    break;
            }

            break;
        }
        case MSP_FLAGS:
        {
            m_inputState = MSP_PAYLOAD_SIZE;
            m_packet.flags = c;
            break;
        }
        case MSP_PAYLOAD_SIZE:
        {
            m_inputState = MSP_PAYLOAD_FUNC;
            m_packet.payloadSize = c;
            m_crc_v1 = CalcCRCxor(&c, 1, m_crc_v1);
            break;
        }
        case MSP_PAYLOAD_FUNC:
        {
            m_inputState = MSP_PAYLOAD;
            m_packet.function = c;
            if (c == 0xff)
            {
                /* this is encapsulated V2 */
                // TODO: check if belongs to me and extract only if needed...
            }
            m_crc_v1 = CalcCRCxor(&c, 1, m_crc_v1);
            break;
        }
        case MSP_PAYLOAD:
        {
            m_packet.addByte(c);
            m_crc_v1 = CalcCRCxor(&c, 1, m_crc_v1);
            if (m_packet.iterated())
            {
                m_crc = m_crc_v1;
                m_inputState = MSP_CHECKSUM;
            }
            break;
        }

        /****************** MSPv2 PARSING **********************/
        case MSP_HEADER_X:
            // Wait for the packet type (cmd or req)
            m_inputState = MSP_HEADER_V2_NATIVE;

            // Start of a new packet
            // reset the packet, offset iterator, and CRC
            m_packet.reset();
            m_offset = 0;
            m_crc = 0;

            switch (c)
            {
                case '<':
                    m_packet.type = MSP_PACKET_V2_COMMAND;
                    break;
                case '>':
                    m_packet.type = MSP_PACKET_V2_RESPONSE;
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
            if (m_offset == sizeof(mspHeaderV2_t))
            {
                // Copy header values into packet
                mspHeaderV2_t *header = (mspHeaderV2_t *)&m_inputBuffer[0];
                m_packet.payloadSize = header->payloadSize;
                m_packet.function = header->function;
                m_packet.flags = header->flags;
                // reset the offset iterator for re-use in payload below
                m_offset = 0;
                m_inputState = MSP_PAYLOAD_V2_NATIVE;
            }
            break;

        case MSP_PAYLOAD_V2_NATIVE:
            // Read bytes until we reach payloadSize
            m_packet.payload[m_offset++] = c;
            m_crc = crc8_dvb_s2(m_crc, c);

            // If we've received the correct amount of bytes for payload
            if (m_offset == m_packet.payloadSize)
            {
                // Then we're up to the CRC
                m_inputState = MSP_CHECKSUM_V2_NATIVE;
            }
            break;

        case MSP_CHECKSUM:
            m_packet.addByte(c);
            // Note: fall to next to check crc
        case MSP_CHECKSUM_V2_NATIVE:
            // Assert that the checksums match
            if (m_crc == c)
            {
                m_inputState = MSP_COMMAND_RECEIVED;
            }
            else
            {
                DEBUG_PRINT("MSP FAIL! CRC: ");
                DEBUG_PRINT(c);
                DEBUG_PRINT(" Exp: ");
                DEBUG_PRINTLN(m_crc);
                m_inputState = MSP_IDLE;
            }
            break;

        default:
            m_inputState = MSP_IDLE;
            break;
    }

    // If we've successfully parsed a complete packet
    // return true so the calling function knows that
    // a new packet is ready.
    return (m_inputState == MSP_COMMAND_RECEIVED);
}

mspPacket_t &MSP::getPacket()
{
    return m_packet;
}

void MSP::markPacketFree()
{
    // Set input state to idle, ready to receive the next packet
    // The current packet data will be discarded internally
    m_inputState = MSP_IDLE;
}

bool MSP::sendPacket(HardwareSerial *port, mspPacketType_e type,
                     uint16_t function, uint8_t flags,
                     uint8_t payloadSize, uint8_t const * payload)
{
    // Sanity check the packet before sending
    if (!payload || !port || type == MSP_PACKET_UNKNOWN)
    {
        // Unsupported packet type (note: ignoring '!' until we know what it is)
        return false;
    }

    if ((type == MSP_PACKET_V1_RESP || type == MSP_PACKET_V2_RESPONSE) && payloadSize == 0)
    {
        // Response packet with no payload
        return false;
    }

    // Write out the framing chars
    port->write('$');
    if (type == MSP_PACKET_V2_RESPONSE || type == MSP_PACKET_V2_COMMAND)
    {
        port->write('X');
    }
    else
    {
        port->write('M');
    }

    // Write out the packet type
    if (type == MSP_PACKET_V1_ELRS)
    {
        port->write('?');
    }
    else if (type == MSP_PACKET_V2_COMMAND || type == MSP_PACKET_V1_CMD)
    {
        port->write('<');
    }
    else
    {
        port->write('>');
    }

    // Subsequent bytes are contained in the crc
    uint8_t crc = 0;

    if (type == MSP_PACKET_V2_RESPONSE || type == MSP_PACKET_V2_COMMAND)
    {
        uint16_t i;
        uint8_t data;
        // Pack header struct into buffer
        uint8_t WORD_ALIGNED_ATTR headerBuffer[sizeof(mspHeaderV2_t)];
        mspHeaderV2_t *header = (mspHeaderV2_t *)&headerBuffer[0];
        header->flags = flags;
        header->function = function;
        header->payloadSize = payloadSize;

        // Write out the header buffer, adding each byte to the crc
        for (i = 0; i < sizeof(headerBuffer); ++i)
        {
            port->write(headerBuffer[i]);
            crc = crc8_dvb_s2(crc, headerBuffer[i]);
        }
        // Write out the payload, adding each byte to the crc
        for (i = 0; i < payloadSize; ++i)
        {
            data = payload[i];
            port->write(data);
            crc = crc8_dvb_s2(crc, data);
        }
    }
    else
    {
        uint8_t data;
        uint8_t payloadsize = (uint8_t)payloadSize;

        if (type == MSP_PACKET_V1_ELRS)
            port->write(flags);

        // payload size
        port->write(payloadsize);
        crc = CalcCRCxor(&payloadsize, 1, 0);

        // frame id
        data = (uint8_t)function;
        port->write(data);
        crc = CalcCRCxor(&data, 1, crc);

        // Write out the payload, adding each byte to the crc
        for (uint8_t i = 0; i < payloadsize; ++i)
        {
            data = payload[i];
            port->write(data);
            crc = CalcCRCxor(&data, 1, crc);
        }
    }

    // Write out the crc
    port->write(crc);

    return true;
}

bool MSP::sendPacket(mspPacket_t *packet, HardwareSerial *port)
{
    return sendPacket(port, packet->type, packet->function, packet->flags, packet->payloadSize, (uint8_t*)packet->payload);
}
