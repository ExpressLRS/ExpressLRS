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

extern int8_t SwitchModePending;

const char *const protoNames[] = {"CRSF", "SBUS", "SUMD"};

static const char *skipChecks;
static Proto proto;
static bool forgedCrcUsed;
static bool decodedInWrongMode;
static unsigned packetsWithoutOutput;
static uint32_t expected[2][3][CRSF_NUM_CHANNELS];

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

// Clears the wrong-mode mark once ChannelData has been reset
static void updateTaint()
{
    for (auto v : ChannelData)
        if (v != CRSF_CHANNEL_VALUE_UNSET)
            return;
    decodedInWrongMode = false;
}

// Unpacks 16 channels of 11 bits, as CRSF and SBUS pack them
static void unpack11(const uint8_t *p, uint32_t *out)
{
    uint32_t bits = 0;
    unsigned have = 0;
    for (unsigned ch = 0; ch < CRSF_NUM_CHANNELS; ch++)
    {
        while (have < 11)
        {
            bits |= (uint32_t)*p++ << have;
            have += 8;
        }
        out[ch] = bits & 0x7ff;
        bits >>= 11;
        have -= 11;
    }
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
    updateTaint();
    if (decodedInWrongMode && !forgedCrcUsed && enabled("mode-mismatch"))
    {
        // Only a value that is wrong in every switch mode counts, the modes quantise switches differently
        for (unsigned ch = 0; ch < CRSF_NUM_CHANNELS; ch++)
        {
            if (ChannelData[ch] == CRSF_CHANNEL_VALUE_UNSET || (proto == PROTO_CRSF && ch >= 14))
                continue;
            bool known = false;
            for (unsigned mode = 0; mode < 3; mode++)
            {
                const uint32_t want = expected[txIsFullRes()][mode][ch];
                known |= emitted[ch] == (proto == PROTO_SUMD ? CRSF_to_US(want) : want);
            }
            if (!known)
                failChannel("mode-mismatch", ch, emitted[ch], expected[txIsFullRes()][tx.mode][ch]);
        }
    }
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
            if (!unset && !forgedCrcUsed && !decodedInWrongMode && emitted[ch] != wantWire && enabled("wrong-value"))
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
    static const eSerialProtocol protocols[] = {PROTOCOL_CRSF, PROTOCOL_SBUS, PROTOCOL_SUMD};
    return config.GetSerialProtocol() != protocols[proto];
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
        while (b.size() - pos >= 2 && b.size() - pos >= 2u + b[pos + 1])
        {
            const size_t len = 2 + b[pos + 1];
            if (b[pos + 2] == CRSF_FRAMETYPE_RC_CHANNELS_PACKED && len == 26)
            {
                unpack11(&b[pos + 3], v);
                checkFrame(v, false);
            }
            pos += len;
        }
    }
    else if (proto == PROTO_SBUS)
    {
        for (; b.size() - pos >= 25; pos += 25)
        {
            if (b[pos] != 0x0F)
                fuzzViolation("harness", "unexpected bytes on the SBUS port");
            unpack11(&b[pos + 1], v);
            checkFrame(v, b[pos + 23] & (1 << 3));
        }
    }
    else
    {
        // SUMD swaps CH5 and CH8 on the wire
        static const uint8_t slotToCh[CRSF_NUM_CHANNELS] = {0, 1, 2, 3, 7, 5, 6, 4, 8, 9, 10, 11, 12, 13, 14, 15};
        for (; b.size() - pos >= 37; pos += 37)
        {
            if (b[pos] != 0xA8)
                fuzzViolation("harness", "unexpected bytes on the SUMD port");
            for (unsigned i = 0; i < CRSF_NUM_CHANNELS; i++)
                v[slotToCh[i]] = ((b[pos + 3 + 2 * i] << 8) | b[pos + 4 + 2 * i]) >> 3;
            checkFrame(v, false);
        }
    }
    b.erase(b.begin(), b.begin() + pos);
}

// Checks made when the RX accepts an RC packet
void checkRcPacket()
{
    updateTaint();
    if (connectionState == connected && !SwitchModePending && !rxDecodesAsTxPacks())
        decodedInWrongMode = true;
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
    isArmed = true;
    for (unsigned full = 0; full < 2; full++)
    {
        const uint8_t size = full ? OTA8_PACKET_SIZE : OTA4_PACKET_SIZE;
        for (unsigned mode = 0; mode < (full ? 3u : 2u); mode++)
        {
            uint32_t *dst = expected[full][mode];
            for (unsigned ch = 0; ch < CRSF_NUM_CHANNELS; ch++)
                dst[ch] = CRSF_CHANNEL_VALUE_UNSET;
            OtaUpdateSerializers((OtaSwitchMode_e)mode, size);
            for (unsigned n = 0; n < 64; n++)
            {
                OTA_Packet_s pkt;
                memset(&pkt, 0, sizeof(pkt));
                OtaNonce = n;
                OtaPackChannelData(&pkt, tx.channels, false);
                OtaUnpackChannelData(&pkt, dst);
            }
        }
    }
    isArmed = false;
    OtaNonce = 0;
}

// Prepares the checks for one test case, whose frames are parsed as outputProtocol
void checksStart(Proto outputProtocol)
{
    skipChecks = getenv("FUZZ_SKIP");
    proto = outputProtocol;
    computeExpected();
}
