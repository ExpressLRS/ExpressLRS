#pragma once

#include <stdint.h>
#include <string.h>

/**
 * @brief Splits a MAVLink byte stream into whole frames.
 *
 * The framer uses only the length fields of the MAVLink v1 and v2 headers.
 * It does not check the CRC, thus it passes the messages of all dialects.
 * Bytes that are not part of a frame are discarded.
 *
 * Complete frames collect in the buffer. flush() gives all complete frames
 * to the send function as one block, and keeps a partial frame for later.
 * push() calls flush() before the next frame can overflow the buffer.
 *
 * @tparam N the size of the buffer, at minimum one frame of the maximum length
 */
template <uint16_t N>
class MavlinkFramer
{
public:
    static constexpr uint16_t MAX_FRAME_LEN = 280; // v2 header 10, payload 255, CRC 2, signature 13
    static_assert(N >= MAX_FRAME_LEN, "MavlinkFramer buffer must hold one frame of the maximum length");

    template <typename SendFn>
    void push(const uint8_t c, SendFn send)
    {
        switch (state)
        {
        case IDLE:
            if (c != STX_V2 && c != STX_V1)
            {
                return;
            }
            if (len + MAX_FRAME_LEN > N)
            {
                flush(send);
            }
            frameStart = len;
            state = (c == STX_V2) ? V2_LEN : V1_LEN;
            break;
        case V1_LEN:
            // seq, sysid, compid, msgid, payload, crc
            remaining = c + 6;
            state = BODY;
            break;
        case V2_LEN:
            payloadLen = c;
            state = V2_INCOMPAT_FLAGS;
            break;
        case V2_INCOMPAT_FLAGS:
            // compat flags, seq, sysid, compid, msgid (3), payload, crc, optional signature
            remaining = payloadLen + 9 + ((c & INCOMPAT_FLAG_SIGNED) ? SIGNATURE_LEN : 0);
            state = BODY;
            break;
        case BODY:
            if (--remaining == 0)
            {
                state = IDLE;
            }
            break;
        }
        buffer[len++] = c;
    }

    template <typename SendFn>
    void flush(SendFn send)
    {
        const uint16_t complete = (state == IDLE) ? len : frameStart;
        if (complete == 0)
        {
            return;
        }
        send(buffer, complete);
        len -= complete;
        memmove(buffer, buffer + complete, len);
        frameStart = 0;
    }

private:
    static constexpr uint8_t STX_V1 = 0xFE;
    static constexpr uint8_t STX_V2 = 0xFD;
    static constexpr uint8_t INCOMPAT_FLAG_SIGNED = 0x01;
    static constexpr uint8_t SIGNATURE_LEN = 13;

    enum State : uint8_t
    {
        IDLE,
        V1_LEN,
        V2_LEN,
        V2_INCOMPAT_FLAGS,
        BODY,
    };

    uint8_t buffer[N];
    uint16_t len = 0;        // bytes in the buffer
    uint16_t frameStart = 0; // start of the partial frame, when state is not IDLE
    uint16_t remaining = 0;  // bytes to come in the partial frame, when state is BODY
    uint8_t payloadLen = 0;
    State state = IDLE;
};
