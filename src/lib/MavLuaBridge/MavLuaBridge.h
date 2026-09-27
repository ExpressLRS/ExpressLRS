// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <stdint.h>
#include <stddef.h>

// Bounded, single-chunk CRSF 0xAA bridge for handset Lua parameter traffic.
//
// All entry points run on the TX main loop; only the handset queue is filled from the
// handset callback. A zero-filled broadcast PING opens a local lease and is consumed
// by the TX, never forwarded to the aircraft.
class MavLuaBridge
{
private:
    // MAVLink v1 message IDs that handset Lua may send upstream.
    enum
    {
        MSG_PING = 4,
        MSG_PARAM_REQUEST_READ = 20,
        MSG_PARAM_SET = 23,
        MSG_COMMAND_LONG = 76,
    };

    // Indexed reads are tracked in a small fixed window; 0xFFFF marks an unused slot.
    static constexpr uint16_t SLOT_EMPTY = 65535;
    static constexpr unsigned SLOT_COUNT = 4;

    // The lease expires when the handset stops sending keepalives; abandoned read
    // reservations are reused after the request timeout.
    static constexpr uint32_t LEASE_MS = 10000;
    static constexpr uint32_t REQUEST_MS = 5000;
    static constexpr uint32_t HEARTBEAT_MS = 3000;
    static constexpr uint16_t MAX_INDEX = 8192;

    // Reads are pipelined at a 20 ms floor; writes keep a wider gap on both sides so
    // PARAM_SET is never interleaved with other traffic. Lua paces reads at 40+ ms.
    static constexpr uint32_t READ_GAP_MS = 20;
    static constexpr uint32_t WRITE_GAP_MS = 100;

    bool active = false;
    bool haveHeartbeat = false;
    bool haveSent = false;
    bool lastWrite = false;
    bool wantVersion = false;

    uint32_t leaseAt = 0;
    uint32_t heartbeatAt = 0;
    uint32_t sentAt = 0;
    uint8_t system = 0;

    uint16_t wanted[SLOT_COUNT] = {SLOT_EMPTY, SLOT_EMPTY, SLOT_EMPTY, SLOT_EMPTY};
    uint32_t requestedAt[SLOT_COUNT] = {};

    uint32_t wantedName = 0;
    uint32_t requestedNameAt = 0;
    uint32_t versionAt = 0;

    // Drops every outstanding named and indexed read reservation.
    void clearReads()
    {
        for (unsigned i = 0; i < SLOT_COUNT; ++i)
        {
            wanted[i] = SLOT_EMPTY;
        }
        wantedName = 0;
    }

    // Returns the MAVLink v1 payload length and CRC_EXTRA seed for an accepted handset
    // message. Unknown IDs are rejected.
    static bool handsetMessage(const uint8_t id, uint8_t &length, uint8_t &crcExtra)
    {
        switch (id)
        {
        case MSG_PING:
            length = 14;
            crcExtra = 237;
            return true;
        case MSG_PARAM_REQUEST_READ:
            length = 20;
            crcExtra = 214;
            return true;
        case MSG_PARAM_SET:
            length = 23;
            crcExtra = 168;
            return true;
        case MSG_COMMAND_LONG:
            length = 33;
            crcExtra = 152;
            return true;
        default:
            return false;
        }
    }

    // Byte offset of target_system within each accepted payload.
    static unsigned targetOffset(const uint8_t id)
    {
        if (id == MSG_PARAM_REQUEST_READ)
        {
            return 2;
        }
        if (id == MSG_PARAM_SET)
        {
            return 4;
        }
        return 30; // COMMAND_LONG
    }

    // FNV-1a over the leading NUL terminated parameter name. Zero is reserved to mean
    // "no named read outstanding", so a genuine hash of zero is mapped to one.
    static uint32_t nameHash(const uint8_t *name)
    {
        uint32_t hash = 2166136261u;
        for (unsigned i = 0; i < 16 && name[i]; ++i)
        {
            hash = (hash ^ name[i]) * 16777619u;
        }
        return hash ? hash : 1;
    }

    // MAVLink CRC-16/MCRF4XX accumulator for one byte.
    static uint16_t crcByte(uint16_t crc, const uint8_t byte)
    {
        uint8_t t = byte ^ (crc & 0xFF);
        t ^= t << 4;
        return (crc >> 8) ^ (uint16_t(t) << 8) ^ (uint16_t(t) << 3) ^ (t >> 4);
    }

    // True while a named read is in flight and within its timeout.
    bool namedPending(const uint32_t now) const
    {
        return wantedName && uint32_t(now - requestedNameAt) < REQUEST_MS;
    }

public:
    // Clears the lease, the discovered target and all outstanding read reservations.
    void reset()
    {
        active = false;
        haveHeartbeat = false;
        haveSent = false;
        lastWrite = false;
        wantVersion = false;
        system = 0;
        clearReads();
    }

    // True while the handset lease is valid and has not expired.
    bool subscribed(const uint32_t now) const
    {
        return active && uint32_t(now - leaseAt) < LEASE_MS;
    }

    // Accepts a handset CRSF 0xAA payload including its chunk marker and data size.
    // Returns the validated MAVLink packet length to forward (starting at payload + 2),
    // or zero when the frame is rejected or consumed locally. The capacity argument is
    // the current uplink FIFO space, so a request is only reserved when it can be queued.
    size_t input(const uint8_t *p, const size_t n, const uint32_t now, const size_t capacity = 58)
    {
        // Envelope: chunk marker 0, data size n-2, then one complete MAVLink packet.
        if (n < 10 || n > 60 || p[0] != 0 || p[1] != n - 2)
        {
            return 0;
        }

        const uint8_t *m = p + 2;
        // Only unsigned MAVLink 1 from handset system/component 254/190 is accepted.
        if (m[0] != 254 || m[3] != 254 || m[4] != 190 || m[1] + 8u != n - 2)
        {
            return 0;
        }

        const uint8_t id = m[5];
        uint8_t length = 0;
        uint8_t crcExtra = 0;
        if (!handsetMessage(id, length, crcExtra) || m[1] != length)
        {
            return 0;
        }

        uint16_t crc = 65535;
        for (size_t i = 1; i < 6u + length; ++i)
        {
            crc = crcByte(crc, m[i]);
        }
        crc = crcByte(crc, crcExtra);
        if (m[6 + length] != (crc & 0xFF) || m[7 + length] != (crc >> 8))
        {
            return 0;
        }

        const uint8_t *data = m + 6;

        if (id == MSG_PING)
        {
            // Only the all-zero keepalive is accepted. It refreshes the local lease and
            // is not forwarded; a heartbeat discovers the target instead.
            for (unsigned i = 0; i < 14; ++i)
            {
                if (data[i])
                {
                    return 0;
                }
            }
            if (!subscribed(now))
            {
                system = 0;
                haveHeartbeat = false;
                wantVersion = false;
                clearReads();
            }
            active = true;
            leaseAt = now;
            return 0;
        }

        // Everything else requires a live lease and a discovered, targetable autopilot.
        if (!subscribed(now) || !system || !haveHeartbeat)
        {
            return 0;
        }
        const uint32_t gap = (id == MSG_PARAM_SET || lastWrite) ? WRITE_GAP_MS : READ_GAP_MS;
        if (haveSent && uint32_t(now - sentAt) < gap)
        {
            return 0;
        }
        if (n - 2 > capacity)
        {
            return 0;
        }

        const unsigned target = targetOffset(id);
        if (data[target] != system || data[target + 1] != 1)
        {
            return 0;
        }
        // Writes additionally require a fresh heartbeat.
        if (id == MSG_PARAM_SET && uint32_t(now - heartbeatAt) > HEARTBEAT_MS)
        {
            return 0;
        }

        if (id == MSG_PARAM_REQUEST_READ)
        {
            const uint16_t index = uint16_t(data[0]) | uint16_t(data[1]) << 8;
            if (index == SLOT_EMPTY)
            {
                // Exact-name read. Only uppercase names are accepted so the reply can be
                // matched by hash without retaining the string.
                unsigned nameLength = 0;
                while (nameLength < 16 && data[4 + nameLength])
                {
                    const uint8_t c = data[4 + nameLength++];
                    const bool valid = (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
                    if (!valid)
                    {
                        return 0;
                    }
                }
                if (!nameLength)
                {
                    return 0;
                }
                // Only one named reservation is tracked; replace it once it has expired.
                const uint32_t hash = nameHash(data + 4);
                if (namedPending(now) && wantedName != hash)
                {
                    return 0;
                }
                wantedName = hash;
                requestedNameAt = now;
            }
            else
            {
                if (index >= MAX_INDEX)
                {
                    return 0;
                }
                // Reuse the matching slot, else the first expired or free slot.
                int slot = -1;
                for (unsigned i = 0; i < SLOT_COUNT; ++i)
                {
                    if (wanted[i] != SLOT_EMPTY && uint32_t(now - requestedAt[i]) >= REQUEST_MS)
                    {
                        wanted[i] = SLOT_EMPTY;
                    }
                    if (wanted[i] == index)
                    {
                        slot = i;
                        break;
                    }
                    if (wanted[i] == SLOT_EMPTY && slot < 0)
                    {
                        slot = i;
                    }
                }
                if (slot < 0)
                {
                    return 0;
                }
                wanted[slot] = index;
                requestedAt[slot] = now;
            }
        }
        else if (id == MSG_COMMAND_LONG)
        {
            // Strict MAV_CMD_REQUEST_MESSAGE(MAVLINK_MSG_ID_AUTOPILOT_VERSION) with no
            // ambiguous options: param1 must be 148.0f, all other params zero, and
            // confirmation must be zero.
            const bool param1IsVersion = data[0] == 0 && data[1] == 0 && data[2] == 0x14 && data[3] == 0x43;
            bool paramsClear = true;
            for (unsigned i = 4; i < 28; ++i)
            {
                if (data[i])
                {
                    paramsClear = false;
                    break;
                }
            }
            const bool commandIsRequestMessage = data[28] == 0 && data[29] == 2;
            if (!param1IsVersion || !paramsClear || !commandIsRequestMessage || data[32])
            {
                return 0;
            }
            wantVersion = true;
            versionAt = now;
        }
        else
        {
            // A different accepted message supersedes outstanding read reservations.
            clearReads();
            wantVersion = false;
        }

        haveSent = true;
        lastWrite = id == MSG_PARAM_SET;
        sentAt = leaseAt = now;
        return n - 2;
    }

    // Filters a downlink message that the existing MAVLink parser already validated; the
    // payload is zero padded by that parser. Only the discovered target is forwarded, and
    // only once per reserved index or name. Returns true when the caller should forward
    // the packet to the handset.
    bool downlink(const uint32_t id, const uint8_t sys, const uint8_t comp, const uint8_t *p, const uint32_t now)
    {
        if (!subscribed(now) || !sys || comp != 1)
        {
            return false;
        }

        if (id == 0 && p[5] == 3)
        {
            // ArduPilot HEARTBEAT locks the target system.
            if (system && sys != system)
            {
                return false;
            }
            system = sys;
            haveHeartbeat = true;
            heartbeatAt = now;
            return true;
        }

        if (sys != system)
        {
            return false;
        }

        if (id == 148 && wantVersion)
        {
            // AUTOPILOT_VERSION reply to our explicit request.
            wantVersion = false;
            return uint32_t(now - versionAt) < REQUEST_MS;
        }

        if (id == 22)
        {
            // PARAM_VALUE. ArduPilot echoes a named read with param_index=-1, so the name
            // is matched before the indexed slots; 0xFFFF also marks a free slot.
            if (wantedName && wantedName == nameHash(p + 8))
            {
                wantedName = 0;
                return uint32_t(now - requestedNameAt) < REQUEST_MS;
            }
            const uint16_t index = uint16_t(p[6]) | uint16_t(p[7]) << 8;
            if (index == SLOT_EMPTY)
            {
                return false;
            }
            for (unsigned i = 0; i < SLOT_COUNT; ++i)
            {
                if (wanted[i] == index)
                {
                    wanted[i] = SLOT_EMPTY;
                    return uint32_t(now - requestedAt[i]) < REQUEST_MS;
                }
            }
        }

        return false;
    }
};

// SYS_STATUS is commonly absent when ArduPilot's EXT_STAT stream is disabled. This
// requests it one-shot while an ArduPilot heartbeat is fresh, but stays silent when the
// aircraft is already streaming it. It is independent of the opt-in parameter
// subscription above.
class MavLuaReadiness
{
private:
    static constexpr uint32_t HEARTBEAT_MS = 3000;
    static constexpr uint32_t STATUS_MS = 2000;
    static constexpr uint32_t REQUEST_MS = 1000;

    uint32_t heartbeatAt = 0;
    uint32_t statusAt = 0;
    uint32_t requestAt = 0;
    uint8_t system = 0;
    bool haveHeartbeat = false;
    bool haveStatus = false;
    bool haveRequest = false;

public:
    // Clears the tracked vehicle and every pacing timestamp.
    void reset()
    {
        heartbeatAt = 0;
        statusAt = 0;
        requestAt = 0;
        system = 0;
        haveHeartbeat = false;
        haveStatus = false;
        haveRequest = false;
    }

    // Tracks heartbeat identity and SYS_STATUS freshness. A different system may only
    // replace the current one once the existing heartbeat has gone stale.
    void observe(const uint32_t id, const uint8_t sys, const uint8_t comp, const uint8_t *p, const uint32_t now)
    {
        if (id == 0 && sys && comp == 1 && p[5] == 3)
        {
            const bool currentIsFresh = haveHeartbeat && uint32_t(now - heartbeatAt) <= HEARTBEAT_MS;
            if (system && sys != system && currentIsFresh)
            {
                return;
            }
            if (sys != system)
            {
                system = sys;
                haveStatus = false;
                haveRequest = false;
            }
            haveHeartbeat = true;
            heartbeatAt = now;
        }
        else if (id == 1 && haveHeartbeat && sys == system && comp == 1)
        {
            haveStatus = true;
            statusAt = now;
        }
    }

    // True when a SYS_STATUS request should be queued: the heartbeat is fresh, the status
    // is missing or stale, and the request pacing interval has elapsed.
    bool due(const uint32_t now) const
    {
        const bool heartbeatFresh = haveHeartbeat && uint32_t(now - heartbeatAt) <= HEARTBEAT_MS;
        const bool statusStale = !haveStatus || uint32_t(now - statusAt) >= STATUS_MS;
        const bool requestDue = !haveRequest || uint32_t(now - requestAt) >= REQUEST_MS;
        return heartbeatFresh && statusStale && requestDue;
    }

    // The discovered autopilot system ID that requests should target.
    uint8_t target() const
    {
        return system;
    }

    // Records that a SYS_STATUS request was queued, starting the pacing interval.
    void requested(const uint32_t now)
    {
        haveRequest = true;
        requestAt = now;
    }
};
