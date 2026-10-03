// Fuzzer for the RX: a simulated TX and lossy radio channel in front of the real rx_main.cpp, with every
// serial frame the RX emits checked against what the TX sent. Usage: make run
//
// The firmware runs on simulated hardware (sim_hardware.cpp). Only the TX is a model: it packs with the real
// OTA code, but its slot, sync and hop scheduling is a simplified copy of tx_main.cpp.
//
// sim_tx.cpp is the TX, fuzz_channel.cpp decides what happens to each packet, fuzz_checks.cpp checks
// what the RX emits. This file plays one test case.

#include <cstdarg>
#include <cstdio>
#include <cstdint>
#include <cstdlib>

#include "common.h"
#include "config.h"
#include "crsf_protocol.h"
#include "elrs_eeprom.h"
#include "OTA.h"

#include "fuzz_channel.h"
#include "fuzz_checks.h"
#include "fuzz_harness.h"
#include "sim_hardware.h"
#include "sim_tx.h"
#include "sim_tx_ota.h"

extern ELRS_EEPROM eeprom;
void setup();
void loop();

constexpr unsigned MAX_SLOTS = 100000;     // seed length cap
constexpr unsigned LOOP_PERIOD_US = 1000;  // between loop() calls

bool fuzzTrace;

// Prints to the per-packet trace, if FUZZ_TRACE is set
void trace(const char *fmt, ...)
{
    if (!fuzzTrace)
        return;
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
}

// Runs the firmware's loop() once, then checks its output
static void mainLoop()
{
    if (rxLoopStalled)
        return;
    loop();
    checkProtocol();
    checkOutput();
}

// Advances simulated time, running loop() once per millisecond
static void advanceTo(uint64_t t)
{
    while (simNow() < t)
    {
        const uint64_t step = simNow() + LOOP_PERIOD_US < t ? simNow() + LOOP_PERIOD_US : t;
        simAdvanceTo(step, checkOutput);
        mainLoop();
    }
}

// True if the RX is listening on the TX's frequency and rate
static bool rxCanHear()
{
    return simRadio.listening && simRadio.freq == txFreq() && simRadio.bw == tx.rate->bw && simRadio.sf == tx.rate->sf &&
        simRadio.cr == tx.rate->cr && simRadio.payloadLength == tx.packetSize;
}

// One TX packet slot, as timerCallback() in tx_main.cpp
static void slot()
{
    txNextSlotTime();
    advanceTo(tx.slotStart);
    rxLoopStalled = false;
    txNextNonce();

    // The TX listens for telemetry on this slot. The RX starts sending at its tock, just before the slot.
    if (txIsTelemetrySlot())
    {
        const uint32_t freq = txFreq();
        advanceTo(tx.slotStart + tx.rate->interval - 1);
        if (simRadio.packetsSent != tx.rxPacketsAtLastTelemetrySlot && simRadio.freq == freq)
        {
            const bool lost = channelTelemetryLost();
            if (!lost)
                tx.lastTelemetryMs = simNow() / 1000;
            trace("%5u %7.1fms TLM  txNonce=%u %s\n", tx.slotNum, simNow() / 1000.0, tx.nonce, lost ? "lost" : "heard");
        }
        tx.rxPacketsAtLastTelemetrySlot = simRadio.packetsSent;
        return;
    }

    OTA_Packet_s pkt;
    txBuildPacket(&pkt);
    const bool wasSync = pkt.std.type == PACKET_TYPE_SYNC;

    advanceTo(tx.slotStart + tx.ratePerf->TOA + channelJitter());
    if (!rxCanHear())
        return;

    Damage damage = {};
    const Fate fate = channelNextFate(&damage);
    channelDamage(fate, &pkt, tx.packetSize, damage);

    // The fuzzer does find real CRC collisions for damaged packets, they are no different to forged ones
    if (fate != DELIVER && fate != DROP)
    {
        txOtaSelect(tx.mode, tx.packetSize, OtaNonce);
        if (txOtaCrcValid(&pkt))
            checksCrcCollision();
    }

    bool accepted = false;
    if (fate != DROP)
    {
        const bool nonceMatched = wasSync || OtaNonce == tx.nonce;
        accepted = simRadioReceive((uint8_t *)&pkt, tx.packetSize);
        // The CRC is seeded with the nonce, so this is one more CRC collision
        if (accepted && !nonceMatched)
            checksCrcCollision();
        if (accepted && pkt.std.type == PACKET_TYPE_RCDATA)
            checkRcPacket();
        checkOutput();
    }
    trace("%5u %7.1fms %s txNonce=%u rxNonce=%u %-10s %s state=%d\n", tx.slotNum, simNow() / 1000.0, wasSync ? "SYNC" : "RC  ", tx.nonce,
        OtaNonce, fateNames[fate], accepted ? "accepted" : "", connectionState);
    mainLoop();
}

// Reads FUZZ_TRACE and sets the channel values the TX sends
void fuzzInit()
{
    fuzzTrace = getenv("FUZZ_TRACE") != nullptr;

    // Distinct values. The TX here always sends the arm flag set. A real TX can take that flag from CH5
    // or from any switch, so CH5 is high to match the case where it comes from CH5.
    constexpr uint32_t FIRST_CHANNEL_VALUE = 300;
    constexpr uint32_t CHANNEL_VALUE_STEP = 80;
    for (unsigned ch = 0; ch < CRSF_NUM_CHANNELS; ch++)
        tx.channels[ch] = FIRST_CHANNEL_VALUE + ch * CHANNEL_VALUE_STEP;
    tx.channels[4] = CRSF_CHANNEL_VALUE_2000;
}

// The count bits of a header byte starting at bit first
static unsigned bits(uint8_t byte, unsigned first, unsigned count)
{
    return (byte >> first) & ((1u << count) - 1);
}

// Boots the RX and plays one test case against it
void fuzzRunInput(const uint8_t *data, size_t size)
{
    // Input layout:
    //   byte  bits  meaning
    //   0     0-7   packet rate, index into the rates that send each packet once
    //   1     0-1   switch mode
    //   1     2-3   output protocol: CRSF, SBUS, SUMD
    //   1     4     failsafe mode: no pulses, last position
    //   1     6-7   telemetry ratio: off, 1:2, 1:8, 1:64
    //   2     0-3   clock offset, -8..+7 eighths of MAX_CLOCK_OFFSET_PPM
    //   2     4-5   clock drift: none, faster, slower, none
    //   2     6     arrival jitter on
    //   3...        opcodes read by channelNextFate()
    if (size < FUZZ_HEADER_BYTES)
        return;

    // Pick the packet rate. The DVDA rates (D500, D250) send every packet 2 or 4 times on different
    // frequencies. The simulated TX cannot do that, so only rates that send each packet once are used.
    uint8_t usableRates[RATE_MAX];
    unsigned usableRateCount = 0;
    for (uint8_t i = 0; i < RATE_MAX; i++)
    {
        const bool sendsEachPacketOnce = get_elrs_airRateConfig(i)->numOfSends == 1;
        if (sendsEachPacketOnce)
            usableRates[usableRateCount++] = i;
    }
    const uint8_t rateIdx = usableRates[data[0] % usableRateCount];
    tx.rate = get_elrs_airRateConfig(rateIdx);
    tx.ratePerf = get_elrs_RFperfParams(rateIdx);
    tx.packetSize = tx.rate->PayloadLength;

    // Link setup: switch mode, output protocol, failsafe mode, telemetry ratio
    const uint8_t header = data[1];
    tx.mode = txAdjustSwitchMode(bits(header, 0, 2) % SWITCH_MODE_COUNT);
    const Proto proto = (Proto)(bits(header, 2, 2) % PROTO_COUNT);
    checksStart(proto);
    const eFailsafeMode failsafeMode = bits(header, 4, 1) ? FAILSAFE_LAST_POSITION : FAILSAFE_NO_PULSES;
    static const expresslrs_tlm_ratio_e ratios[] = {TLM_RATIO_NO_TLM, TLM_RATIO_1_2, TLM_RATIO_1_8, TLM_RATIO_1_64};
    tx.tlmRatio = ratios[bits(header, 6, 2)];
    tx.tlmDenom = TLMratioEnumToValue(tx.tlmRatio);

    // How far off the TX's clock is, and how much arrival times vary
    const uint8_t timing = data[2];
    constexpr int CLOCK_OFFSET_STEPS = 8; // each way from zero
    tx.clockOffsetMilliPpm = ((int)bits(timing, 0, 4) - CLOCK_OFFSET_STEPS) * MAX_CLOCK_OFFSET_PPM * 1000 / CLOCK_OFFSET_STEPS;
    static const int driftDirection[] = {0, 1, -1, 0};
    tx.clockDriftMilliPpmPerS = driftDirection[bits(timing, 4, 2)] * MAX_CLOCK_DRIFT_PPM_PER_S * 1000;
    const int arrivalJitterUs = bits(timing, 6, 1) ? MAX_ARRIVAL_JITTER_US : 0;
    channelStart(data, size, FUZZ_HEADER_BYTES, arrivalJitterUs);

    // Clock and Serial hooks must be in place before any firmware code runs
    simInstall();

    // What the user would have set up through binding and the configurator
    static uint8_t uid[UID_LEN] = {0, 0, 0x12, 0x34, 0x56, 0x78};
    config.SetStorageProvider(&eeprom);
    config.Load();
    config.SetUID(uid);
    config.SetSerialProtocol(protoConfigValues[proto]);
    config.SetFailsafeMode(failsafeMode);
    config.SetRateInitialIdx(rateIdx);
    config.Commit();

    // Boot the RX, and drop anything it printed while starting
    setup();
    simSerialOut.clear();

    txStart(simNow(), uid);
    trace("%s %uHz %s txMode=%d tlm=1:%u failsafe=%d clock=%+dppm drift=%+dppm/s jitter=%dus\n", protoNames[proto],
        1000000 / tx.rate->interval, txIsFullRes() ? "fullres" : "std", tx.mode, tx.tlmDenom, failsafeMode,
        tx.clockOffsetMilliPpm / 1000, tx.clockDriftMilliPpmPerS / 1000, arrivalJitterUs);

    // One TX packet slot at a time, until the input runs out
    for (unsigned n = 0; n < MAX_SLOTS && channelActive(); n++)
        slot();
}
