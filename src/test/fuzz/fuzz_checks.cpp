#include "fuzz_checks.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "common.h"
#include "config.h"
#include "crsf_protocol.h"
#include "OTA.h"

#include "fuzz_harness.h"
#include "sim_hardware.h"
#include "sim_tx.h"
#include "sim_tx_ota.h"

extern int8_t SwitchModePending;

const char *const protoNames[] = {"CRSF", "SBUS", "SUMD"};
const eSerialProtocol protoConfigValues[] = {PROTOCOL_CRSF, PROTOCOL_SBUS, PROTOCOL_SUMD};

static std::string skipList; // FUZZ_SKIP with a comma at each end
static Proto proto;
static bool forgedCrcUsed;
static unsigned packetsWithoutOutput;
static uint32_t lastFrame[CRSF_NUM_CHANNELS]; // the last frame that was not flagged failsafe
static bool lastFrameValid;
// What the RX should hold for each channel: the packets it accepted, unpacked by the TX's OTA copy
static uint32_t rxShouldHold[CRSF_NUM_CHANNELS];
static unsigned unflaggedFramesSinceLinkLost;

// True when the RX unpacks the way the TX packs
static bool rxDecodesAsTxPacks()
{
    // The sync packet only has one bit for the switch mode, so this is all the RX can ever know
    return OtaIsFullRes == txIsFullRes() && OtaSwitchModeCurrent == (tx.mode & 1);
}

// True when the RX has no switch mode change pending either
static bool modesAgree()
{
    return rxDecodesAsTxPacks() && SwitchModePending == 0;
}

// False if FUZZ_SKIP lists the check, as "leak" or "leak:SBUS"
static bool enabled(const std::string &check)
{
    const std::string scoped = check + ":" + protoNames[proto];
    return skipList.find("," + check + ",") == std::string::npos && skipList.find("," + scoped + ",") == std::string::npos;
}

// The 16 channel values in a row, with "-" for unset if asked
static std::string channelList(const uint32_t *channels, bool dashForUnset)
{
    std::string list;
    for (unsigned ch = 0; ch < CRSF_NUM_CHANNELS; ch++)
    {
        const bool dash = dashForUnset && channels[ch] == CRSF_CHANNEL_VALUE_UNSET;
        list += dash ? " -" : " " + std::to_string(channels[ch]);
    }
    return list;
}

// Reports a violation and ends the test case
[[noreturn]] static void fail(const char *check, const char *what)
{
    static const char *const stateNames[] = {"connected", "tentative", "awaiting model id", "disconnected"};
    const char *state = connectionState <= disconnected ? stateNames[connectionState] : "other";
    char where[128];
    snprintf(where, sizeof(where), "slot %u, %uHz %s, txMode=%d rxMode=%d, %s", tx.slotNum, 1000000 / tx.rate->interval,
        txIsFullRes() ? "fullres" : "std", tx.mode, OtaSwitchModeCurrent, state);

    const std::string kind = std::string(check) + ":" + protoNames[proto];
    const std::string report = std::string("VIOLATION ") + check + " on " + protoNames[proto] +
        "\n  where     " + where +
        "\n  what      " + what +
        "\n  channels " + channelList(ChannelData, true);
    fuzzViolation(kind, report);
}

// Reports a violation about one channel of an emitted frame
[[noreturn]] static void failChannel(const char *check, unsigned ch, uint32_t got, uint32_t want)
{
    char what[64];
    snprintf(what, sizeof(what), "CH%u emitted %u, expected %u", ch + 1, got, want);
    fail(check, what);
}

// The 16 channels of a frame's packed channel block, which CRSF and SBUS share
static void unpackChannels(const uint8_t *packed, uint32_t *out)
{
    crsf_channels_s c;
    memcpy(&c, packed, sizeof(c));
    const uint32_t channels[CRSF_NUM_CHANNELS] = {c.ch0, c.ch1, c.ch2,  c.ch3,  c.ch4,  c.ch5,  c.ch6,  c.ch7,
                                                  c.ch8, c.ch9, c.ch10, c.ch11, c.ch12, c.ch13, c.ch14, c.ch15};
    memcpy(out, channels, sizeof(channels));
}

// What a channel the RX has received reads on the wire
static uint32_t expectedOnWire(unsigned ch)
{
    return proto == PROTO_SUMD ? CRSF_to_US(rxShouldHold[ch]) : rxShouldHold[ch];
}

// In failsafe mode "last position", a frame flagged failsafe holds each channel where it was
static void checkFailsafeFrame(const uint32_t *emitted)
{
    if (config.GetFailsafeMode() != FAILSAFE_LAST_POSITION || !lastFrameValid || forgedCrcUsed)
        return;
    for (unsigned ch = 0; ch < CRSF_NUM_CHANNELS; ch++)
    {
        // A packet after the last frame may have brought the channel's value
        const bool heldLast = emitted[ch] == lastFrame[ch];
        const bool receivedSince = txSendsChannel(ch) && emitted[ch] == expectedOnWire(ch);
        if (!heldLast && !receivedSince && enabled("failsafe-not-held"))
            failChannel("failsafe-not-held", ch, emitted[ch], lastFrame[ch]);
    }
}

// Runs the checks on one RC frame the RX emitted
static void checkFrame(const uint32_t *emitted, bool flaggedFailsafe)
{
    if (fuzzTrace)
        trace("    %s frame%s:%s\n", protoNames[proto], flaggedFailsafe ? " (failsafe)" : "", channelList(emitted, false).c_str());
    if (flaggedFailsafe)
    {
        checkFailsafeFrame(emitted);
        return;
    }
    if (connectionState == disconnected)
    {
        // One frame may go out between the link dropping and the driver being told. It carries the
        // last values, which may have been decoded in a switch mode the RX has since left.
        if (++unflaggedFramesSinceLinkLost > 1 && enabled("failsafe-not-flagged"))
            fail("failsafe-not-flagged", "the link is lost, but RC frames keep going out without the failsafe flag");
        return;
    }
    memcpy(lastFrame, emitted, sizeof(lastFrame));
    lastFrameValid = true;
    packetsWithoutOutput = 0;
    if (!modesAgree())
        return;

    const bool crsfLinkStatsChannels = proto == PROTO_CRSF && !(txIsFullRes() && OtaSwitchModeCurrent == smHybridOr16ch);
    for (unsigned ch = 0; ch < CRSF_NUM_CHANNELS; ch++)
    {
        if (crsfLinkStatsChannels && ch >= 14)
            continue;
        const bool unset = ChannelData[ch] == CRSF_CHANNEL_VALUE_UNSET;
        const uint32_t minVal = proto == PROTO_SUMD ? CRSF_to_US(CRSF_CHANNEL_VALUE_EXT_MIN) : CRSF_CHANNEL_VALUE_EXT_MIN;
        if (txSendsChannel(ch))
        {
            if (unset && enabled("leak"))
            {
                char what[64];
                snprintf(what, sizeof(what), "CH%u emitted %u, but has not been received", ch + 1, emitted[ch]);
                fail("leak", what);
            }
            if (!unset && !forgedCrcUsed && emitted[ch] != expectedOnWire(ch) && enabled("wrong-value"))
                failChannel("wrong-value", ch, emitted[ch], expectedOnWire(ch));
        }
        else if (unset && emitted[ch] != minVal && enabled("unset-not-min"))
        {
            failChannel("unset-not-min", ch, emitted[ch], minVal);
        }
    }
}

// True if the RX has left the configured serial protocol
static bool protocolChanged()
{
    return config.GetSerialProtocol() != protoConfigValues[proto];
}

// Fails if the RX has left the configured serial protocol
void checkProtocol()
{
    if (protocolChanged() && enabled("protocol-changed"))
    {
        char what[64];
        snprintf(what, sizeof(what), "serial protocol is now %d", config.GetSerialProtocol());
        fail("protocol-changed", what);
    }
}

// CRSF frame: sync, length of what follows, type, payload, CRC
constexpr size_t CRSF_TYPE_AT = 2;
constexpr size_t CRSF_PAYLOAD_AT = 3;
constexpr size_t CRSF_RC_FRAME_LEN = CRSF_FRAME_NOT_COUNTED_BYTES + CRSF_FRAME_SIZE(sizeof(crsf_channels_s));

// SBUS frame: header, packed channels, flags, footer
constexpr size_t SBUS_FRAME_LEN = 25;
constexpr uint8_t SBUS_HEADER = 0x0F;
constexpr size_t SBUS_CHANNELS_AT = 1;
constexpr size_t SBUS_FLAGS_AT = 23;
constexpr uint8_t SBUS_FLAG_FAILSAFE = 1 << 3;

// SUMD frame: 3 header bytes, 16 big-endian channels in eighths of a microsecond, CRC16
constexpr size_t SUMD_FRAME_LEN = 37;
constexpr uint8_t SUMD_HEADER = 0xA8;
constexpr size_t SUMD_CHANNELS_AT = 3;
constexpr unsigned SUMD_EIGHTHS_SHIFT = 3;

// Splits the captured serial bytes into frames and checks each
void checkRxFrames()
{
    if (connectionState != disconnected)
        unflaggedFramesSinceLinkLost = 0;
    std::vector<uint8_t> &b = simSerialOut;
    if (protocolChanged())
    {
        b.clear();
        return;
    }
    uint32_t v[CRSF_NUM_CHANNELS];
    size_t pos = 0;
    if (proto == PROTO_CRSF)
    {
        // Link statistics and other telemetry frames share the port with the RC frames
        while (b.size() - pos >= CRSF_FRAME_NOT_COUNTED_BYTES && b.size() - pos >= CRSF_FRAME_NOT_COUNTED_BYTES + b[pos + 1])
        {
            const size_t len = CRSF_FRAME_NOT_COUNTED_BYTES + b[pos + 1];
            if (b[pos + CRSF_TYPE_AT] == CRSF_FRAMETYPE_RC_CHANNELS_PACKED && len == CRSF_RC_FRAME_LEN)
            {
                unpackChannels(&b[pos + CRSF_PAYLOAD_AT], v);
                checkFrame(v, false);
            }
            pos += len;
        }
    }
    else if (proto == PROTO_SBUS)
    {
        for (; b.size() - pos >= SBUS_FRAME_LEN; pos += SBUS_FRAME_LEN)
        {
            if (b[pos] != SBUS_HEADER)
                fuzzViolation("harness", "unexpected bytes on the SBUS port");
            unpackChannels(&b[pos + SBUS_CHANNELS_AT], v);
            checkFrame(v, b[pos + SBUS_FLAGS_AT] & SBUS_FLAG_FAILSAFE);
        }
    }
    else
    {
        // SUMD swaps CH5 and CH8 on the wire
        static const uint8_t slotToCh[CRSF_NUM_CHANNELS] = {0, 1, 2, 3, 7, 5, 6, 4, 8, 9, 10, 11, 12, 13, 14, 15};
        for (; b.size() - pos >= SUMD_FRAME_LEN; pos += SUMD_FRAME_LEN)
        {
            if (b[pos] != SUMD_HEADER)
                fuzzViolation("harness", "unexpected bytes on the SUMD port");
            for (unsigned i = 0; i < CRSF_NUM_CHANNELS; i++)
            {
                const uint8_t *value = &b[pos + SUMD_CHANNELS_AT + 2 * i];
                v[slotToCh[i]] = ((value[0] << 8) | value[1]) >> SUMD_EIGHTHS_SHIFT;
            }
            checkFrame(v, false);
        }
    }
    b.erase(b.begin(), b.begin() + pos);
}

// Checks made when the RX accepts an RC packet. asSent is false if the packet was damaged or the
// RX was at another nonce than the TX.
void checkRcPacket(const OTA_Packet_s *pkt, bool asSent)
{
    if (asSent)
    {
        txOtaSelect(tx.mode, tx.packetSize, tx.nonce);
        txOtaUnpackChannels(pkt, rxShouldHold);
    }
    if (connectionState == connected && connectionHasModelMatch && modesAgree())
    {
        bool allReceived = true;
        for (unsigned ch = 0; ch < CRSF_NUM_CHANNELS; ch++)
            allReceived &= !txSendsChannel(ch) || ChannelData[ch] != CRSF_CHANNEL_VALUE_UNSET;
        packetsWithoutOutput = allReceived ? packetsWithoutOutput + 1 : 0;
        if (packetsWithoutOutput > BLACKOUT_PACKETS && enabled("blackout"))
            fail("blackout", "valid RC packets keep arriving but no RC frame goes out");
    }
}

// A damaged packet passed the CRC, so the value checks no longer hold
void checksCrcCollision()
{
    forgedCrcUsed = true;
}

// Prepares the checks for one test case, whose frames are parsed as outputProtocol
void checksStart(Proto outputProtocol)
{
    const char *skip = getenv("FUZZ_SKIP");
    skipList = std::string(",") + (skip ? skip : "") + ",";
    proto = outputProtocol;
    for (auto &value : rxShouldHold)
        value = CRSF_CHANNEL_VALUE_UNSET;
}
