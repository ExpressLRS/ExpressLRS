// SPDX-License-Identifier: GPL-3.0-or-later
#if defined(TARGET_TX)

#include "MavLuaTransport.h"
#include "MavLuaBridge.h"

#include "CRSFHandset.h"
#include "CRSFRouter.h"
#include "FIFO.h"
#include "common/mavlink.h"
#include "config.h"
#include "rxtx_intf.h"

#include <algorithm>
#include <string.h>

extern FIFO<1024> uartInputBuffer;
extern TxConfig config;

// Legacy TBS MAVLink-over-CRSF envelope, used only between the handset and the TX module.
static constexpr uint8_t MAVLUA_FRAME_TYPE = 0xAA;
// frame_size bounds for a single-chunk envelope carrying one MAVLink packet.
static constexpr uint8_t MAVLUA_MIN_FRAME_SIZE = 12;
static constexpr uint8_t MAVLUA_MAX_FRAME_SIZE = 62;
// Largest downlink packet accepted, and the CRSF chunk size used to carry it.
static constexpr uint16_t MAVLUA_DOWNLINK_MAX = 16 * 58;
static constexpr uint8_t MAVLUA_CHUNK_SIZE = 58;

// Handset frames are copied here by the handset callback and drained on the TX main loop.
static FIFO<128> mavLuaQueue;
static MavLuaBridge mavLuaBridge;
static MavLuaReadiness mavLuaReadiness;

bool mavLuaEnqueue(const uint8_t *frame)
{
    if (frame[2] != MAVLUA_FRAME_TYPE)
    {
        return false;
    }
    const uint8_t frameSize = frame[1];
    if (frameSize < MAVLUA_MIN_FRAME_SIZE || frameSize > MAVLUA_MAX_FRAME_SIZE)
    {
        return true; // Claimed, but unusable: drop it rather than letting the router route it.
    }
    // Copy the payload, excluding the type byte and the CRSF CRC, with a length prefix.
    const uint8_t length = frameSize - 2;
    mavLuaQueue.lock();
    if (mavLuaQueue.free() >= length + 1)
    {
        mavLuaQueue.push(length);
        mavLuaQueue.pushBytes(frame + 3, length);
    }
    mavLuaQueue.unlock();
    return true;
}

void mavLuaPoll()
{
    const bool enabled = config.GetLinkMode() == TX_MAVLINK_MODE && connectionState == connected;
    if (!enabled)
    {
        mavLuaBridge.reset();
        mavLuaReadiness.reset();
        mavLuaQueue.lock();
        mavLuaQueue.flush();
        mavLuaQueue.unlock();
        return;
    }

    const uint32_t now = millis();

    // Drain at most one queued handset frame per loop to keep the callback bounded.
    uint8_t data[60];
    uint8_t length = 0;
    mavLuaQueue.lock();
    if (mavLuaQueue.size())
    {
        length = mavLuaQueue.pop();
        mavLuaQueue.popBytes(data, length);
    }
    mavLuaQueue.unlock();

    if (length)
    {
        uartInputBuffer.lock();
        // Reserve the request only when the existing uplink FIFO can hold it.
        const size_t forward = mavLuaBridge.input(data, length, now, uartInputBuffer.free());
        if (forward)
        {
            uartInputBuffer.pushBytes(data + 2, forward);
        }
        uartInputBuffer.unlock();
    }

    if (mavLuaReadiness.due(now))
    {
        mavlink_message_t msg;
        uint8_t packet[MAVLINK_MAX_PACKET_LEN];
        mavlink_msg_command_long_pack(254, 190, &msg, mavLuaReadiness.target(),
                                      MAV_COMP_ID_AUTOPILOT1, MAV_CMD_REQUEST_MESSAGE, 0,
                                      MAVLINK_MSG_ID_SYS_STATUS, 0, 0, 0, 0, 0, 0);
        const uint16_t packetLength = mavlink_msg_to_send_buffer(packet, &msg);
        uartInputBuffer.lock();
        if (uartInputBuffer.free() >= packetLength)
        {
            uartInputBuffer.pushBytes(packet, packetLength);
            mavLuaReadiness.requested(now);
        }
        uartInputBuffer.unlock();
    }
}

void mavLuaObserve(uint32_t id, uint8_t sys, uint8_t comp, const uint8_t *payload)
{
    mavLuaReadiness.observe(id, sys, comp, payload, millis());
}

void mavLuaDownlink(uint32_t id, uint8_t sys, uint8_t comp, const uint8_t *payload,
                    const uint8_t *packet, uint16_t length)
{
    if (!length || length > MAVLUA_DOWNLINK_MAX
        || !mavLuaBridge.downlink(id, sys, comp, payload, millis()))
    {
        return;
    }

    // Split the packet into bounded CRSF chunks. The marker packs the last chunk index in
    // the high nibble and the current index in the low nibble.
    const uint8_t chunks = (length + MAVLUA_CHUNK_SIZE - 1) / MAVLUA_CHUNK_SIZE;
    for (uint8_t current = 0; current < chunks; ++current)
    {
        uint8_t frame[MAVLUA_CHUNK_SIZE + 6] = {};
        const uint16_t offset = uint16_t(current) * MAVLUA_CHUNK_SIZE;
        const uint8_t part = std::min<uint16_t>(MAVLUA_CHUNK_SIZE, length - offset);
        frame[3] = uint8_t((chunks - 1) << 4) | current;
        frame[4] = part;
        memcpy(frame + 5, packet + offset, part);
        auto header = reinterpret_cast<crsf_header_t *>(frame);
        crsfRouter.SetHeaderAndCrc(header, static_cast<crsf_frame_type_e>(MAVLUA_FRAME_TYPE), part + 4);
        crsfRouter.deliverMessageTo(CRSF_ADDRESS_RADIO_TRANSMITTER, header);
    }
}
#endif

