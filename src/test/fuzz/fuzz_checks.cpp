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

static const char *skipChecks;
static Proto proto;
static bool forgedCrcUsed;
static unsigned packetsWithoutOutput;
// What each channel reads once received, by packet format (std, full-res) and switch mode
static uint32_t expected[2][SWITCH_MODE_COUNT][CRSF_NUM_CHANNELS];
// Packets in a row that carry every channel at least once, in any switch mode. The slowest is
// Hybrid, which sends one of 7 switches per packet.
constexpr unsigned PACKETS_FOR_ALL_CHANNELS = 64;

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
static bool enabled(const char *check)
{
    if (!skipChecks)
        return true;
    char scoped[64];
    snprintf(scoped, sizeof(scoped), "%s:%s", check, protoNames[proto]);
    for (const char *tok = skipChecks; *tok;)
    {
        const size_t len = strcspn(tok, ",");
        if ((len == strlen(check) && !strncmp(tok, check, len)) || (len == strlen(scoped) && !strncmp(tok, scoped, len)))
            return false;
        tok += len + (tok[len] == ',');
    }
    return true;
}

// Reports a violation and ends the test case
[[noreturn]] static void fail(const char *check, const char *what)
{
    static const char *const stateNames[] = {"connected", "tentative", "awaiting model id", "disconnected"};
    const char *state = connectionState <= disconnected ? stateNames[connectionState] : "other";
    char buf[512];
    int n = snprintf(buf, sizeof(buf),
        "VIOLATION %s on %s\n"
        "  where     slot %u, %uHz %s, txMode=%d rxMode=%d, %s\n"
        "  what      %s\n"
        "  channels ",
        check, protoNames[proto], tx.slotNum, 1000000 / tx.rate->interval, txIsFullRes() ? "fullres" : "std", tx.mode,
        OtaSwitchModeCurrent, state, what);
    for (auto v : ChannelData)
    {
        if (v == CRSF_CHANNEL_VALUE_UNSET)
            n += snprintf(buf + n, sizeof(buf) - n, " -");
        else
            n += snprintf(buf + n, sizeof(buf) - n, " %u", v);
    }
    fuzzViolation(std::string(check) + ":" + protoNames[proto], buf);
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

// Runs the checks on one RC frame the RX emitted
static void checkFrame(const uint32_t *emitted, bool flaggedFailsafe)
{
    if (fuzzTrace)
    {
        fprintf(stderr, "    %s frame%s:", protoNames[proto], flaggedFailsafe ? " (failsafe)" : "");
        for (unsigned ch = 0; ch < CRSF_NUM_CHANNELS; ch++)
            fprintf(stderr, " %u", emitted[ch]);
        fprintf(stderr, "\n");
    }
    // The frame that goes out between the link dropping and the driver being told carries the last
    // values, which may have been decoded in a switch mode the RX has since left
    if (flaggedFailsafe || connectionState == disconnected)
        return;
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
            const uint32_t want = expected[txIsFullRes()][tx.mode][ch];
            const uint32_t wantWire = proto == PROTO_SUMD ? CRSF_to_US(want) : want;
            if (unset && enabled("leak"))
                failChannel("leak", ch, emitted[ch], wantWire);
            if (!unset && !forgedCrcUsed && emitted[ch] != wantWire && enabled("wrong-value"))
                failChannel("wrong-value", ch, emitted[ch], wantWire);
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
void checkOutput()
{
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

// Checks made when the RX accepts an RC packet
void checkRcPacket()
{
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

// Works out what each channel reads after a clean round trip
static void computeExpected()
{
    for (unsigned full = 0; full < 2; full++)
    {
        const uint8_t size = full ? OTA8_PACKET_SIZE : OTA4_PACKET_SIZE;
        // sm12ch is full-res only
        const unsigned modes = full ? SWITCH_MODE_COUNT : SWITCH_MODE_COUNT - 1;
        for (unsigned mode = 0; mode < modes; mode++)
        {
            uint32_t *dst = expected[full][mode];
            for (unsigned ch = 0; ch < CRSF_NUM_CHANNELS; ch++)
                dst[ch] = CRSF_CHANNEL_VALUE_UNSET;
            for (unsigned n = 0; n < PACKETS_FOR_ALL_CHANNELS; n++)
            {
                OTA_Packet_s pkt;
                memset(&pkt, 0, sizeof(pkt));
                txOtaSelect((OtaSwitchMode_e)mode, size, n);
                txOtaPackChannels(&pkt, tx.channels);
                txOtaUnpackChannels(&pkt, dst);
            }
        }
    }
}

// Prepares the checks for one test case, whose frames are parsed as outputProtocol
void checksStart(Proto outputProtocol)
{
    skipChecks = getenv("FUZZ_SKIP");
    proto = outputProtocol;
    computeExpected();
}
