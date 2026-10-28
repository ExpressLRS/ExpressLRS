#include <cstdint>
#include <cstdio>
#include <cmath>
#include <vector>
#include <algorithm>
#include <unity.h>

#include "rx-connect/RateFingerprint.h"
#include "../../src/rx-connect/RateFingerprint.cpp"
#include "SX12xxDriverCommon.h"

using namespace RateFingerprint;

// A rate's fields that make its signature, from ExpressLRS_AirRateConfig (band, hop interval,
// interval) and ExpressLRS_AirRateRFperf (TOA, and TOA2 for dual band) in src/common.cpp
struct TableRate
{
    RadioBandMod::Combined radio_type;
    uint32_t toa;
    uint8_t FHSShopInterval;
    uint32_t interval;
    const char *name;
    uint32_t toa2;
};

static const TableRate lr1121Rates[] = {
    {RadioBandMod::GFSK_900,    453, 2,  1000, "FSK 900 1000 8ch"},
    {RadioBandMod::LORA_900,   2960, 4,  4000, "LoRa 900 250"},
    {RadioBandMod::LORA_900,   3664, 4,  5000, "LoRa 900 200 8ch"},
    {RadioBandMod::LORA_900,   4380, 4,  5000, "LoRa 900 200"},
    {RadioBandMod::LORA_900,   6690, 4, 10000, "LoRa 900 100 8ch"},
    {RadioBandMod::LORA_900,   8770, 4, 10000, "LoRa 900 100"},
    {RadioBandMod::LORA_900,  18560, 4, 20000, "LoRa 900 50"},
    {RadioBandMod::LORA_900,  29950, 2, 40000, "LoRa 900 25"},
    {RadioBandMod::LORA_900,   4380, 2,  5000, "LoRa 900 50 DVDA"},
    {RadioBandMod::GFSK_2G4,    480, 2,  1000, "FSK 2G4 1000"},
    {RadioBandMod::GFSK_2G4,    480, 2,  1000, "FSK 2G4 500 DVDA"},
    {RadioBandMod::GFSK_2G4,    480, 2,  1000, "FSK 2G4 250 DVDA"},
    {RadioBandMod::LORA_2G4,   1507, 4,  2000, "LoRa 2G4 500"},
    {RadioBandMod::LORA_2G4,   2374, 4,  3003, "LoRa 2G4 333 8ch"},
    {RadioBandMod::LORA_2G4,   3300, 4,  4000, "LoRa 2G4 250"},
    {RadioBandMod::LORA_2G4,   5871, 4,  6666, "LoRa 2G4 150"},
    {RadioBandMod::LORA_2G4,   7605, 4, 10000, "LoRa 2G4 100 8ch"},
    {RadioBandMod::LORA_2G4,  10798, 2, 20000, "LoRa 2G4 50"},
    {RadioBandMod::LORA_DUAL,  5152, 4,  6666, "Dual 150", 5081},
    {RadioBandMod::LORA_DUAL,  7968, 4, 10000, "Dual 100 8ch", 7916},
};
static constexpr size_t RATE_COUNT = sizeof(lr1121Rates) / sizeof(lr1121Rates[0]);

static const TableRate sx127xRates[] = {
    {RadioBandMod::LORA_900,  4380, 4,  5000, "200"},
    {RadioBandMod::LORA_900,  6690, 4, 10000, "100 8ch"},
    {RadioBandMod::LORA_900,  8770, 4, 10000, "100"},
    {RadioBandMod::LORA_900, 18560, 4, 20000, "50"},
    {RadioBandMod::LORA_900, 29950, 2, 40000, "25"},
    {RadioBandMod::LORA_900,  4380, 2,  5000, "50 DVDA"},
};

static const TableRate sx1280Rates[] = {
    {RadioBandMod::FLRC_2G4,   389, 2,  1000, "F1000"},
    {RadioBandMod::FLRC_2G4,   389, 2,  2000, "F500"},
    {RadioBandMod::FLRC_2G4,   389, 2,  1000, "D500"},
    {RadioBandMod::FLRC_2G4,   389, 2,  1000, "D250"},
    {RadioBandMod::LORA_2G4,  1507, 4,  2000, "500"},
    {RadioBandMod::LORA_2G4,  2374, 4,  3003, "333 8ch"},
    {RadioBandMod::LORA_2G4,  3300, 4,  4000, "250"},
    {RadioBandMod::LORA_2G4,  5871, 4,  6666, "150"},
    {RadioBandMod::LORA_2G4,  7605, 4, 10000, "100 8ch"},
    {RadioBandMod::LORA_2G4, 10798, 2, 20000, "50"},
};

static const TableRate lr2021Rates[] = {
    {RadioBandMod::GFSK_900,    453, 2,  1000, "FSK 900 1000 8ch"},
    {RadioBandMod::LORA_900,   2960, 4,  4000, "LoRa 900 250"},
    {RadioBandMod::LORA_900,   3664, 4,  5000, "LoRa 900 200 8ch"},
    {RadioBandMod::LORA_900,   4380, 4,  5000, "LoRa 900 200"},
    {RadioBandMod::LORA_900,   6690, 4, 10000, "LoRa 900 100 8ch"},
    {RadioBandMod::LORA_900,   8770, 4, 10000, "LoRa 900 100"},
    {RadioBandMod::LORA_900,  18560, 4, 20000, "LoRa 900 50"},
    {RadioBandMod::LORA_900,  29950, 2, 40000, "LoRa 900 25"},
    {RadioBandMod::LORA_900,   4380, 2,  5000, "LoRa 900 50 DVDA"},
    {RadioBandMod::FLRC_2G4,    389, 2,  1000, "FLRC 1000"},
    {RadioBandMod::FLRC_2G4,    389, 2,  2000, "FLRC 500"},
    {RadioBandMod::FLRC_2G4,    389, 2,  1000, "FLRC 500 DVDA"},
    {RadioBandMod::FLRC_2G4,    389, 2,  1000, "FLRC 250 DVDA"},
    {RadioBandMod::GFSK_2G4,    480, 2,  1000, "FSK 2G4 1000"},
    {RadioBandMod::GFSK_2G4,    480, 2,  1000, "FSK 2G4 500 DVDA"},
    {RadioBandMod::GFSK_2G4,    480, 2,  1000, "FSK 2G4 250 DVDA"},
    {RadioBandMod::LORA_2G4,   1507, 4,  2000, "LoRa 2G4 500"},
    {RadioBandMod::LORA_2G4,   2374, 4,  3003, "LoRa 2G4 333 8ch"},
    {RadioBandMod::LORA_2G4,   3300, 4,  4000, "LoRa 2G4 250"},
    {RadioBandMod::LORA_2G4,   5871, 4,  6666, "LoRa 2G4 150"},
    {RadioBandMod::LORA_2G4,   7605, 4, 10000, "LoRa 2G4 100 8ch"},
    {RadioBandMod::LORA_2G4,  10798, 2, 20000, "LoRa 2G4 50"},
    {RadioBandMod::LORA_DUAL,  5152, 4,  6666, "Dual 150", 5081},
    {RadioBandMod::LORA_DUAL,  7968, 4, 10000, "Dual 100 8ch", 7916},
};

// FCC915 and ISM2G4
static const uint8_t channelCount[SLOT_COUNT] = {40, 80};

static RateSignature tableSignature(const TableRate &r, uint8_t index)
{
    uint32_t airtimeUs[SLOT_COUNT] = {};
    if (RadioBandMod::isBDUAL(r.radio_type))
    {
        airtimeUs[SLOT_900] = r.toa;
        airtimeUs[SLOT_2G4] = r.toa2;
    }
    else
    {
        airtimeUs[RadioBandMod::isB2G4(r.radio_type) ? SLOT_2G4 : SLOT_900] = r.toa;
    }
    return makeSignature(index, airtimeUs, r.interval, r.FHSShopInterval, channelCount);
}

template <size_t N>
static std::vector<RateSignature> tableSignatures(const TableRate (&rates)[N], std::vector<const char *> *names = nullptr)
{
    std::vector<RateSignature> sigs;
    for (uint8_t i = 0; i < N; i++)
    {
        sigs.push_back(tableSignature(rates[i], i));
        if (names)
            names->push_back(rates[i].name);
    }
    return sigs;
}

static RateSignature lr1121Signature(uint8_t index)
{
    return tableSignature(lr1121Rates[index], index);
}

static std::vector<RateSignature> allSignatures()
{
    return tableSignatures(lr1121Rates);
}

static bool sameOnAir(const RateSignature &a, const RateSignature &b)
{
    for (uint8_t slot = 0; slot < SLOT_COUNT; slot++)
    {
        if (a.onBand[slot] != b.onBand[slot] || a.airtimeUs[slot] != b.airtimeUs[slot])
            return false;
    }
    return a.intervalUs == b.intervalUs && a.hopInterval == b.hopInterval;
}

void test_lr1121_train(void)
{
    static const uint32_t expectedTrain[RATE_COUNT] = {
        2000, 16000, 20000, 20000, 40000, 40000, 80000, 80000, 10000,
        2000, 2000, 2000, 8000, 12012, 16000, 26664, 40000, 40000,
        26664, 40000,
    };
    for (uint8_t i = 0; i < RATE_COUNT; i++)
    {
        const RateSignature sig = lr1121Signature(i);
        TEST_ASSERT_EQUAL_UINT32(expectedTrain[i], sig.trainUs);
        // The sync channel is visited once per pass through every channel
        if (sig.onBand[SLOT_900])
            TEST_ASSERT_EQUAL_UINT32(expectedTrain[i] * 40, sig.revisitUs[SLOT_900]);
        if (sig.onBand[SLOT_2G4])
            TEST_ASSERT_EQUAL_UINT32(expectedTrain[i] * 80, sig.revisitUs[SLOT_2G4]);
    }
}

void test_lr1121_identical_on_air(void)
{
    // Only the 2.4GHz FSK rates, which differ just in numOfSends, can't be told apart by RF
    const std::vector<RateSignature> sigs = allSignatures();
    for (uint8_t a = 0; a < RATE_COUNT; a++)
    {
        for (uint8_t b = a + 1; b < RATE_COUNT; b++)
        {
            const bool expectSame = a >= 9 && b <= 11;
            TEST_ASSERT_EQUAL_MESSAGE(expectSame, sameOnAir(sigs[a], sigs[b]), lr1121Rates[b].name);
        }
    }
}

// Report the closest pairs on each band, and the feature that best splits them
void test_lr1121_separability_report(void)
{
    const std::vector<RateSignature> sigs = allSignatures();
    const Tuning tuning;
    printf("\nLR1121 separability at %uus resolution (z = difference / sigma)\n", tuning.resolutionUs);
    for (uint8_t a = 0; a < RATE_COUNT; a++)
    {
        for (uint8_t b = a + 1; b < RATE_COUNT; b++)
        {
            for (uint8_t slot = 0; slot < SLOT_COUNT; slot++)
            {
                if (!sigs[a].onBand[slot] || !sigs[b].onBand[slot] || sameOnAir(sigs[a], sigs[b]))
                    continue;
                if (sigs[a].trainUs != sigs[b].trainUs && sigs[a].intervalUs != sigs[b].intervalUs)
                    continue; // trivially separable
                if (sigs[a].onBand[!slot] != sigs[b].onBand[!slot])
                    continue; // separable by whether there is anything on the other band
                const float airtimeSigma = fmaxf(tuning.airtimeSigmaFrac * sigs[a].airtimeUs[slot], tuning.resolutionUs);
                const float intervalSigma = fmaxf(tuning.intervalSigmaFrac * sigs[a].intervalUs, tuning.resolutionUs);
                const float zAirtime = fabsf((float)sigs[a].airtimeUs[slot] - sigs[b].airtimeUs[slot]) / airtimeSigma;
                const float zInterval = fabsf((float)sigs[a].intervalUs - sigs[b].intervalUs) / intervalSigma;
                const char *split = sigs[a].hopInterval != sigs[b].hopInterval ? "packets per train"
                    : zInterval > zAirtime ? "interval" : "airtime";
                printf("  %-18s vs %-18s %s: airtime z=%.1f interval z=%.1f, split by %s\n",
                    lr1121Rates[a].name, lr1121Rates[b].name, slot == SLOT_900 ? "900" : "2G4", zAirtime, zInterval, split);
                // Everything not identical on air must be separable by at least one feature
                TEST_ASSERT_TRUE(sigs[a].hopInterval != sigs[b].hopInterval || zAirtime > 3 || zInterval > 3);
            }
        }
    }
}

// Deterministic LCG so the synthetic captures are repeatable
static uint32_t rngState;
static uint32_t rng(uint32_t max)
{
    rngState = rngState * 1664525 + 1013904223;
    return (rngState >> 8) % max;
}

struct SyntheticCapture
{
    std::vector<Burst> bursts[SLOT_COUNT];
    BandCapture captures[SLOT_COUNT];
};

// What the scorer can see of the true rate in a synthetic capture
struct Structure
{
    uint32_t pairedTrains = 0;  // complete trains with at least two packets
    uint32_t loneTrains = 0;    // complete trains with one packet
    uint32_t maxPackets = 0;

    // Enough for the scorer: two trains of at least two packets on the grid, or one train of at
    // least three packets. Lone packets are too easily made by clutter to count.
    bool enough(uint8_t hopInterval) const
    {
        return maxPackets >= 3 || pairedTrains >= 2;
    }
};

/**
 * Bursts the sync channel would show for a TX on sig: one train every revisit period, edges
 * quantised to the RSSI poll resolution, and no packet in the telemetry slot, every tlmDenom'th
 * packet (0 for no telemetry)
 * @return what the capture shows of the rate on its best band
 */
static Structure synthesise(SyntheticCapture &out, const RateSignature &sig, uint32_t windowUs, uint32_t resolutionUs, uint32_t tlmDenom)
{
    Structure best;
    // Packet slots are counted from an arbitrary point before the window
    const uint32_t slotOffset = rng(1000);
    for (uint8_t slot = 0; slot < SLOT_COUNT; slot++)
    {
        Structure structure;
        std::vector<Burst> &bursts = out.bursts[slot];
        bursts.clear();
        if (sig.onBand[slot])
        {
            for (uint32_t trainStart = rng(sig.revisitUs[slot]) / sig.intervalUs * sig.intervalUs; trainStart < windowUs; trainStart += sig.revisitUs[slot])
            {
                uint32_t packets = 0;
                for (uint8_t k = 0; k < sig.hopInterval; k++)
                {
                    uint32_t start = trainStart + k * sig.intervalUs;
                    if (start >= windowUs)
                        break;
                    if (tlmDenom && (slotOffset + start / sig.intervalUs) % tlmDenom == 0)
                        continue;
                    // As measured on the bench: starts jitter by tens of us, and the RSSI decays
                    // slowly so bursts measure 100-400us longer than the packet
                    uint32_t end = start + sig.airtimeUs[slot] + 100 + rng(300);
                    start += rng(60);
                    // Quantise to the poll grid, edges seen on the first poll after they happen
                    start = (start / resolutionUs + 1) * resolutionUs;
                    end = std::min((end / resolutionUs + 1) * resolutionUs, windowUs);
                    bursts.push_back({start, end});
                    ++packets;
                }
                if (trainStart >= sig.trainUs && trainStart + 2 * sig.trainUs <= windowUs)
                {
                    structure.pairedTrains += packets >= 2;
                    structure.loneTrains += packets == 1;
                    structure.maxPackets = std::max(structure.maxPackets, packets);
                }
            }
        }
        out.captures[slot] = {true, 0, windowUs, bursts.data(), (uint16_t)bursts.size()};
        if (structure.enough(sig.hopInterval))
            best = structure;
    }
    return best;
}

// Rates the RSSI fingerprint can't tell apart, so a test can't expect one ranked above the other:
// the same interval on the same bands, with airtimes closer than the RSSI tail. That covers rates
// differing only in packets per hop (e.g. 3 and 8, which the scan visits together anyway) and, on
// the LR2021, FLRC (388us) against GFSK (480us) at 2.4GHz.
static bool sameScanVisit(const RateSignature &a, const RateSignature &b)
{
    const uint32_t tail = Tuning().rssiTailUs;
    for (uint8_t slot = 0; slot < SLOT_COUNT; slot++)
    {
        if (a.onBand[slot] != b.onBand[slot])
            return false;
        const uint32_t diff = a.airtimeUs[slot] > b.airtimeUs[slot] ? a.airtimeUs[slot] - b.airtimeUs[slot] : b.airtimeUs[slot] - a.airtimeUs[slot];
        if (diff >= tail)
            return false;
    }
    return a.intervalUs == b.intervalUs;
}

static size_t rankOf(const std::vector<RateSignature> &sigs, const uint8_t *order, size_t count, uint8_t truth)
{
    // Rank among rates the scan visits separately
    size_t rank = 0;
    for (size_t i = 0; i < count; i++)
    {
        if (sameScanVisit(sigs[order[i]], sigs[truth]))
            return rank;
        ++rank;
    }
    return rank;
}

// Rank synthetic captures of every rate in a chip's table, at several TLM ratios, poll resolutions,
// and with and without a wrong prior
static void rankSynthetic(const char *chip, const std::vector<RateSignature> &sigs, const std::vector<const char *> &names)
{
    const size_t rates = sigs.size();
    bool candidate[MAX_RATES];
    for (bool &c : candidate) c = true;

    Tuning tuning;
    uint32_t checked = 0, withoutStructure = 0;
    for (uint32_t resolutionUs : {30u, 60u, 100u})
    {
        tuning.resolutionUs = resolutionUs;
        for (uint32_t tlmDenom : {2u, 8u, 128u})
        {
            for (uint8_t truth = 0; truth < rates; truth++)
            {
                for (int prior : {-1, (int)(truth + 1) % (int)rates})
                {
                    rngState = truth * 7919 + resolutionUs * 31 + tlmDenom * 7 + prior;
                    // Enough time to see several trains on the slowest band of the true rate
                    uint32_t revisit = 0;
                    for (uint8_t slot = 0; slot < SLOT_COUNT; slot++)
                        revisit = std::max(revisit, sigs[truth].revisitUs[slot]);
                    SyntheticCapture capture;
                    const Structure structure = synthesise(capture, sigs[truth], revisit * 3, resolutionUs, tlmDenom);

                    uint8_t order[MAX_RATES];
                    float scores[MAX_RATES];
                    const size_t count = rankRates(sigs.data(), candidate, rates, capture.captures, prior, tuning, order, scores);
                    TEST_ASSERT_EQUAL(rates, count);

                    char msg[160];
                    snprintf(msg, sizeof(msg), "%s truth %s, TLM 1:%u, prior %d, resolution %uus, top %s",
                        chip, names[truth], tlmDenom, prior, resolutionUs, names[order[0]]);
                    ++checked;
                    if (structure.enough(sigs[truth].hopInterval))
                    {
                        TEST_ASSERT_EQUAL_MESSAGE(0, rankOf(sigs, order, count, truth), msg);
                    }
                    else
                    {
                        // e.g. a hop 2 rate at TLM 1:2 only ever shows lone packets. Not enough to
                        // pick it out, but nothing else may claim it either
                        ++withoutStructure;
                        if (prior >= 0)
                            TEST_ASSERT_TRUE_MESSAGE(order[0] == prior || sameScanVisit(sigs[order[0]], sigs[truth]), msg);
                        for (size_t i = 0; i < count; i++)
                            TEST_ASSERT_TRUE_MESSAGE(scores[order[i]] <= 0 || order[i] == prior || sameScanVisit(sigs[order[i]], sigs[truth]), msg);
                    }
                }
            }
        }
    }
    printf("\n%s synthetic: %u cases, %u without enough structure to identify the rate\n", chip, checked, withoutStructure);
}

static std::vector<const char *> lr1121Names()
{
    std::vector<const char *> names;
    for (const TableRate &r : lr1121Rates)
        names.push_back(r.name);
    return names;
}

void test_rank_synthetic(void)
{
    rankSynthetic("LR1121", allSignatures(), lr1121Names());
}

void test_nothing_detected_falls_back_to_prior(void)
{
    const std::vector<RateSignature> sigs = allSignatures();
    bool candidate[RATE_COUNT];
    for (bool &c : candidate) c = true;
    candidate[18] = candidate[19] = false; // single LR1121, no dual band

    // Nothing listened to yet
    const BandCapture none[SLOT_COUNT] = {};
    uint8_t order[RATE_COUNT];
    const size_t count = rankRates(sigs.data(), candidate, RATE_COUNT, none, 6, Tuning(), order);

    TEST_ASSERT_EQUAL(RATE_COUNT - 2, count);
    // Prior first, then the quickest to try: shortest sync channel revisit, ties in table order
    TEST_ASSERT_EQUAL(6, order[0]);
    auto revisit = [&](uint8_t r) {
        const RateSignature &s = sigs[r];
        return std::max(s.onBand[SLOT_900] ? s.revisitUs[SLOT_900] : 0, s.onBand[SLOT_2G4] ? s.revisitUs[SLOT_2G4] : 0);
    };
    for (size_t i = 2; i < count; i++)
    {
        TEST_ASSERT_TRUE_MESSAGE(revisit(order[i - 1]) < revisit(order[i])
            || (revisit(order[i - 1]) == revisit(order[i]) && order[i - 1] < order[i]), lr1121Rates[order[i]].name);
    }
}

// Having listened to the 900MHz sync channel and seen nothing, try the 2.4GHz rates first, the
// prior still first of all
void test_empty_band_tried_last(void)
{
    const std::vector<RateSignature> sigs = allSignatures();
    bool candidate[RATE_COUNT];
    for (bool &c : candidate) c = true;
    candidate[18] = candidate[19] = false;

    const BandCapture captures[SLOT_COUNT] = {{true, 0, 1000000, nullptr, 0}, {}};
    uint8_t order[RATE_COUNT];
    const size_t count = rankRates(sigs.data(), candidate, RATE_COUNT, captures, 6, Tuning(), order);

    TEST_ASSERT_EQUAL(6, order[0]);
    bool seen900 = false;
    for (size_t i = 1; i < count; i++)
    {
        const bool is900 = sigs[order[i]].onBand[SLOT_900];
        TEST_ASSERT_FALSE_MESSAGE(seen900 && !is900, lr1121Rates[order[i]].name);
        seen900 |= is900;
    }
}

void test_clutter_does_not_sink_truth(void)
{
    // Short WiFi/BLE-like bursts scattered through the window
    const std::vector<RateSignature> sigs = allSignatures();
    bool candidate[RATE_COUNT];
    for (bool &c : candidate) c = true;

    for (uint8_t truth = 0; truth < RATE_COUNT; truth++)
    {
        rngState = 1234 + truth;
        uint32_t revisit = std::max(sigs[truth].revisitUs[SLOT_900], sigs[truth].revisitUs[SLOT_2G4]);
        const uint32_t window = revisit * 3;
        SyntheticCapture capture;
        synthesise(capture, sigs[truth], window, 30, 64);

        std::vector<Burst> merged[SLOT_COUNT];
        for (uint8_t slot = 0; slot < SLOT_COUNT; slot++)
        {
            merged[slot] = capture.bursts[slot];
            for (int n = 0; n < 4; n++)
            {
                const uint32_t start = rng(window - 1000);
                merged[slot].push_back({start, start + 100 + rng(300)});
            }
            std::sort(merged[slot].begin(), merged[slot].end(), [](const Burst &a, const Burst &b) { return a.startUs < b.startUs; });
            capture.captures[slot].bursts = merged[slot].data();
            capture.captures[slot].count = merged[slot].size();
        }

        uint8_t order[RATE_COUNT];
        const size_t count = rankRates(sigs.data(), candidate, RATE_COUNT, capture.captures, -1, Tuning(), order);
        char msg[64];
        snprintf(msg, sizeof(msg), "truth %s, top %s", lr1121Rates[truth].name, lr1121Rates[order[0]].name);
        TEST_ASSERT_LESS_OR_EQUAL_MESSAGE(1, rankOf(sigs, order, count, truth), msg);
    }
}

// The first 16 of 101 bursts captured on an LR1121 RX bench setup with the TX at 2.4GHz 100Hz 8ch
// (rate 16), polling RSSI every 29us. Most are short WiFi/BLE-like clutter.
void test_bench_capture_2g4_100hz_8ch(void)
{
    const std::vector<RateSignature> sigs = allSignatures();
    bool candidate[RATE_COUNT];
    for (bool &c : candidate) c = true;
    candidate[18] = candidate[19] = false; // single LR1121, no dual band

    static const Burst bursts[] = {
        {0, 7040}, {9314, 17055}, {19315, 27029}, {55160, 55391}, {87771, 88090}, {129731, 129789},
        {131432, 131490}, {133257, 133315}, {149315, 157068}, {159307, 167026}, {169315, 177345},
        {179327, 187575}, {209661, 209955}, {215228, 215924}, {289258, 289518}, {311910, 311968},
    };
    const BandCapture captures[SLOT_COUNT] = {
        {true, 0, 1000000, nullptr, 0},
        {true, 0, 1000000, bursts, sizeof(bursts) / sizeof(bursts[0])},
    };
    Tuning tuning;
    tuning.resolutionUs = 29;

    for (int prior : {-1, 16, 5})
    {
        uint8_t order[RATE_COUNT];
        float scores[RATE_COUNT];
        const size_t count = rankRates(sigs.data(), candidate, RATE_COUNT, captures, prior, tuning, order, scores);
        char msg[64];
        snprintf(msg, sizeof(msg), "prior %d, top %s %.1f vs %.1f", prior, lr1121Rates[order[0]].name, scores[order[0]], scores[16]);
        TEST_ASSERT_EQUAL_MESSAGE(0, rankOf(sigs, order, count, 16), msg);
        if (prior == -1)
        {
            printf("\nBench capture ranking, no prior:\n");
            for (size_t i = 0; i < 4; i++)
                printf("  %u. %-18s %.1f\n", (unsigned)i + 1, lr1121Rates[order[i]].name, scores[order[i]]);
        }
    }
}

// With the TX off, clutter on its own should rarely look like a rate. A clear match scores 20 or
// more, so that must be rare even with very dense clutter: here about 200
// bursts a second, 0.1-2.5ms long, ~27% duty. LoRa 2.4GHz 500Hz (87% duty) is the easiest to fake,
// from back-to-back clutter that happens to be chopped into ~2ms pieces.
void test_clutter_alone_rarely_matches_clearly(void)
{
    const std::vector<RateSignature> sigs = allSignatures();
    const Tuning tuning;
    constexpr int SEEDS = 500;
    constexpr float CLEAR_MATCH = 20;
    int reachedClear = 0;
    float worst = 0;

    for (int seed = 0; seed < SEEDS; seed++)
    {
        rngState = seed;
        std::vector<Burst> bursts[SLOT_COUNT];
        for (uint8_t slot = 0; slot < SLOT_COUNT; slot++)
        {
            for (uint32_t t = rng(5000); t < 1000000; t += 100 + rng(10000))
            {
                const uint32_t len = 100 + rng(2400);
                bursts[slot].push_back({t, std::min(t + len, 1000000u)});
                t += len;
            }
        }
        const BandCapture captures[SLOT_COUNT] = {
            {true, 0, 1000000, bursts[SLOT_900].data(), (uint16_t)bursts[SLOT_900].size()},
            {true, 0, 1000000, bursts[SLOT_2G4].data(), (uint16_t)bursts[SLOT_2G4].size()},
        };

        float best = 0;
        for (uint8_t i = 0; i < RATE_COUNT; i++)
            best = std::max(best, scoreLikelihood(sigs[i], captures, tuning));
        reachedClear += best >= CLEAR_MATCH;
        worst = std::max(worst, best);
    }
    printf("\nClutter alone: %d of %d captures matched a rate clearly, worst %.1f\n", reachedClear, SEEDS, worst);
    TEST_ASSERT_LESS_OR_EQUAL(SEEDS / 50, reachedClear);
}

// Second bench capture, TX at 2.4GHz 100Hz 8ch, RX configured for LoRa 2.4GHz 100Hz 8ch
void test_bench_capture_2g4_100hz_8ch_second(void)
{
    const std::vector<RateSignature> sigs = allSignatures();
    bool candidate[RATE_COUNT];
    for (bool &c : candidate) c = true;
    candidate[18] = candidate[19] = false;

    static const Burst bursts[] = {
        {0, 6071}, {7878, 15745}, {17871, 25671}, {37890, 37976}, {65525, 65766}, {87892, 87978},
        {107914, 116037}, {117928, 125716}, {127843, 135723}, {137935, 145727}, {157936, 158023},
        {177946, 178179}, {185577, 185663}, {205592, 205684}, {225583, 225670}, {226303, 226771},
    };
    const BandCapture captures[SLOT_COUNT] = {
        {true, 0, 1000000, nullptr, 0},
        {true, 0, 1000000, bursts, sizeof(bursts) / sizeof(bursts[0])},
    };
    Tuning tuning;
    tuning.resolutionUs = 29;

    for (int prior : {-1, 5})
    {
        uint8_t order[RATE_COUNT];
        const size_t count = rankRates(sigs.data(), candidate, RATE_COUNT, captures, prior, tuning, order);
        TEST_ASSERT_EQUAL_MESSAGE(0, rankOf(sigs, order, count, 16), lr1121Rates[order[0]].name);
    }
}

// Feed the detector a square wave description: (time, rssi) changes, sampled every 29us
static uint16_t detect(Burst *out, uint16_t capacity, const std::vector<std::pair<uint32_t, int8_t>> &levels,
                       uint32_t windowUs, BurstDetector **detectorOut = nullptr)
{
    static BurstDetector *detector;
    delete detector;
    detector = new BurstDetector(out, capacity, -90, 3, 100, 200);
    size_t level = 0;
    for (uint32_t t = 2; t < windowUs; t += 29)
    {
        while (level + 1 < levels.size() && levels[level + 1].first <= t)
            ++level;
        detector->sample(t, levels[level].second);
    }
    if (detectorOut)
        *detectorOut = detector;
    return detector->finish(windowUs);
}

void test_burst_detector(void)
{
    Burst out[8];
    BurstDetector *detector;

    // One packet: start and end both recorded (the end was lost on the bench, giving huge lengths)
    TEST_ASSERT_EQUAL(1, detect(out, 8, {{0, -110}, {1000, -50}, {3000, -110}}, 10000));
    TEST_ASSERT_UINT32_WITHIN(30, 1000, out[0].startUs);
    TEST_ASSERT_UINT32_WITHIN(30, 3000, out[0].endUs);

    // A dip shorter than the merge gap is ripple within the packet
    TEST_ASSERT_EQUAL(1, detect(out, 8, {{0, -110}, {1000, -50}, {2000, -110}, {2060, -50}, {3000, -110}}, 10000));
    TEST_ASSERT_UINT32_WITHIN(30, 3000, out[0].endUs);

    // A longer gap splits it into two packets
    TEST_ASSERT_EQUAL(2, detect(out, 8, {{0, -110}, {1000, -50}, {2000, -110}, {2300, -50}, {3000, -110}}, 10000));
    TEST_ASSERT_UINT32_WITHIN(30, 2000, out[0].endUs);
    TEST_ASSERT_UINT32_WITHIN(30, 2300, out[1].startUs);

    // Noise spikes shorter than the minimum are dropped
    TEST_ASSERT_EQUAL(0, detect(out, 8, {{0, -110}, {1000, -50}, {1090, -110}}, 10000, &detector));
    TEST_ASSERT_EQUAL(1, detector->dropped());

    // Bursts clipped by the window edges are kept, starting at 0 / ending at the window end
    TEST_ASSERT_EQUAL(2, detect(out, 8, {{0, -50}, {60, -110}, {9950, -50}}, 10000));
    TEST_ASSERT_EQUAL_UINT32(0, out[0].startUs);
    TEST_ASSERT_EQUAL_UINT32(10000, out[1].endUs);

    // Stops recording when full
    std::vector<std::pair<uint32_t, int8_t>> many;
    for (uint32_t t = 0; t < 20000; t += 1000)
    {
        many.push_back({t, -50});
        many.push_back({t + 500, -110});
    }
    TEST_ASSERT_EQUAL(4, detect(out, 4, many, 20000, &detector));
    TEST_ASSERT_TRUE(detector->full());

    // Sliding along: dropping the oldest keeps the rest in order, including a burst still open
    BurstDetector sliding(out, 5, -90, 3, 100, 200);
    for (uint32_t t = 0; t < 4500; t += 29)
        sliding.sample(t, (t % 1000) < 500 ? -50 : -110); // bursts at 0 (clipped), 1000, 2000, 3000; 4000 open
    TEST_ASSERT_EQUAL(4, sliding.count());
    TEST_ASSERT_UINT32_WITHIN(30, 500, sliding.dropOldest(1));
    TEST_ASSERT_EQUAL(3, sliding.count());
    TEST_ASSERT_UINT32_WITHIN(30, 1000, out[0].startUs);
    TEST_ASSERT_EQUAL(4, sliding.finish(4500));

    // A strong signal that never drops below the threshold makes one long burst. Raising the
    // threshold lets the dips between packets end it
    BurstDetector strong(out, 8, -90, 3, 100, 200);
    for (uint32_t t = 0; t < 3000; t += 29)
        strong.sample(t, (t % 1000) < 700 ? -30 : -70); // packets at -30, leakage at -70 between them
    TEST_ASSERT_UINT32_WITHIN(30, 3000, strong.openBurstUs(3000));
    strong.setThreshold(-60);
    TEST_ASSERT_EQUAL(-60, strong.getThreshold());
    for (uint32_t t = 3000; t < 6000; t += 29)
        strong.sample(t, (t % 1000) < 700 ? -30 : -70);
    // The long burst ends at the first dip after the change, then each packet is its own burst
    TEST_ASSERT_EQUAL(3, strong.finish(6000));
    TEST_ASSERT_UINT32_WITHIN(30, 3700, out[0].endUs);
    TEST_ASSERT_UINT32_WITHIN(30, 4000, out[1].startUs);
    TEST_ASSERT_UINT32_WITHIN(30, 4700, out[1].endUs);
    TEST_ASSERT_UINT32_WITHIN(30, 4000, out[3].startUs);
    TEST_ASSERT_EQUAL_UINT32(4500, out[3].endUs);
}

// Bench capture with the TX at 2.4GHz FSK 250Hz DVDA and the RX configured for LoRa 2.4GHz 333Hz.
// The 480us FSK packets measure 590-700us, and with the strong signal neighbouring channels leak
// in as runs of packets 1ms apart.
void test_bench_capture_2g4_fsk_dvda(void)
{
    const std::vector<RateSignature> sigs = allSignatures();
    bool candidate[RATE_COUNT];
    for (bool &c : candidate) c = true;
    candidate[18] = candidate[19] = false;

    static const Burst bursts[] = {
        {1014344, 1015010}, {1015344, 1015982}, {1018354, 1018991}, {1019346, 1019983}, {1022354, 1023002},
        {1023331, 1024026}, {1024355, 1024974}, {1025356, 1025993}, {1026349, 1026986}, {1027368, 1027958},
        {1030354, 1030996}, {1031344, 1031986}, {1035179, 1035411}, {1048365, 1048978}, {1049355, 1049979},
        {1050356, 1054423},
    };
    const BandCapture captures[SLOT_COUNT] = {
        {},
        {true, 1000000, 1056000, bursts, sizeof(bursts) / sizeof(bursts[0])},
    };
    Tuning tuning;
    tuning.resolutionUs = 29;

    uint8_t order[RATE_COUNT];
    float scores[RATE_COUNT];
    const size_t count = rankRates(sigs.data(), candidate, RATE_COUNT, captures, 13, tuning, order, scores);
    char msg[64];
    snprintf(msg, sizeof(msg), "top %s %.1f, FSK %.1f", lr1121Rates[order[0]].name, scores[order[0]], scores[11]);
    TEST_ASSERT_EQUAL_MESSAGE(0, rankOf(sigs, order, count, 11), msg);
    // A clear match
    TEST_ASSERT_TRUE_MESSAGE(scores[11] >= 20, msg);
}

void test_sx127x(void)
{
    std::vector<const char *> names;
    rankSynthetic("SX127x", tableSignatures(sx127xRates, &names), names);
}

void test_sx1280(void)
{
    std::vector<const char *> names;
    rankSynthetic("SX1280", tableSignatures(sx1280Rates, &names), names);
}

void test_lr2021(void)
{
    std::vector<const char *> names;
    rankSynthetic("LR2021", tableSignatures(lr2021Rates, &names), names);
}

// Bench capture on an SX1280 RX (RP1, ESP8285) with the TX at 2.4GHz LoRa 333Hz 8ch, polling RSSI
// every 61us. The 2374us packets measure 2475-2690us with the RSSI tail.
void test_bench_capture_sx1280_333hz_8ch(void)
{
    std::vector<const char *> names;
    const std::vector<RateSignature> sigs = tableSignatures(sx1280Rates, &names);
    bool candidate[MAX_RATES];
    for (bool &c : candidate) c = true;

    static const Burst bursts[] = {
        {68367, 68795}, {141991, 144466}, {144958, 147465}, {147954, 150522}, {150950, 153457},
        {156942, 159632}, {159938, 162629}, {162996, 165567}, {172505, 172933}, {213593, 214204},
        {237447, 240567}, {241059, 243595}, {244085, 246653}, {247020, 249589}, {254052, 254296},
        {285747, 286113},
    };
    const BandCapture captures[SLOT_COUNT] = {
        {},
        {true, 0, 300000, bursts, sizeof(bursts) / sizeof(bursts[0])},
    };
    Tuning tuning;
    tuning.resolutionUs = 61;

    uint8_t order[MAX_RATES];
    float scores[MAX_RATES];
    const size_t count = rankRates(sigs.data(), candidate, sigs.size(), captures, -1, tuning, order, scores);
    char msg[64];
    snprintf(msg, sizeof(msg), "top %s %.1f, 333 8ch %.1f", names[order[0]], scores[order[0]], scores[5]);
    TEST_ASSERT_EQUAL_MESSAGE(0, rankOf(sigs, order, count, 5), msg);
    // A clear match
    TEST_ASSERT_TRUE_MESSAGE(scores[5] >= 20, msg);
}

// Bench, XR4 with the TX close by on 2.4GHz K1000: its packets leak in from its other channels, so
// the 2.4GHz sync channel shows a burst every 2-4ms rather than the rate's pairs, and 900MHz hasn't
// been listened to. The two packets per hop rates can't be ruled out by that, so they must not sink
// below the untried 900MHz rates (which before this cost the slow 900MHz dwells, ~7s)
void test_leakage_does_not_rule_out_two_per_hop(void)
{
    const std::vector<RateSignature> sigs = allSignatures();
    bool candidate[RATE_COUNT];
    for (bool &c : candidate) c = true;

    std::vector<Burst> bursts;
    for (uint32_t t = 2000; t + 700 < 690000; t += (t / 2000) % 3 ? 2000 : 4000)
        bursts.push_back({t, t + 622});
    const BandCapture captures[SLOT_COUNT] = {{}, {true, 0, 690000, bursts.data(), (uint16_t)bursts.size()}};

    uint8_t order[RATE_COUNT];
    float scores[RATE_COUNT];
    const size_t count = rankRates(sigs.data(), candidate, RATE_COUNT, captures, -1, Tuning(), order, scores);

    // Not matching isn't evidence against it, so no absence penalty
    TEST_ASSERT_EQUAL_FLOAT(0, scores[9]);
    // Tried before the slow 900MHz rates, and before the 2.4GHz LoRa rates it beat on the air
    TEST_ASSERT_TRUE(rankOf(sigs, order, count, 9) < rankOf(sigs, order, count, 6));
    TEST_ASSERT_TRUE(rankOf(sigs, order, count, 9) < rankOf(sigs, order, count, 7));
    TEST_ASSERT_TRUE(rankOf(sigs, order, count, 9) < rankOf(sigs, order, count, 14));
}

// A silent band still rules out the two packets per hop rates
void test_silent_band_rules_out_two_per_hop(void)
{
    const std::vector<RateSignature> sigs = allSignatures();
    const BandCapture captures[SLOT_COUNT] = {{}, {true, 0, 690000, nullptr, 0}};
    TEST_ASSERT_TRUE(scoreLikelihood(sigs[9], captures, Tuning()) < 0);
}

// Bench, XR4: saved on 900MHz 200Hz with the TX on 900MHz 250Hz. The capture before leaving 200Hz
// was shorter than 250Hz's sync channel revisit, so 250Hz has a small absence penalty, while 25Hz
// and 2.4GHz 50Hz (two packets per hop) can't be ruled out and score 0. Those cost ~3.5s each to
// try, so the cheap 250Hz must still go first (they went first and took ~11s)
void test_cheap_weak_penalty_before_expensive_unknown(void)
{
    const std::vector<RateSignature> sigs = allSignatures();
    bool candidate[RATE_COUNT];
    for (bool &c : candidate) c = true;
    candidate[3] = false; // the saved rate, already tried

    // Clutter that matches nothing
    const Burst bursts[] = {{20000, 20300}, {95000, 96100}, {181000, 181250}, {260000, 262000}, {377000, 377400}};
    const BandCapture captures[SLOT_COUNT] = {{true, 0, 400000, bursts, 5}, {}};

    uint8_t order[RATE_COUNT];
    float scores[RATE_COUNT];
    const size_t count = rankRates(sigs.data(), candidate, RATE_COUNT, captures, -1, Tuning(), order, scores);

    TEST_ASSERT_TRUE(scores[1] < 0);
    TEST_ASSERT_EQUAL_FLOAT(0, scores[7]);
    TEST_ASSERT_TRUE(rankOf(sigs, order, count, 1) < rankOf(sigs, order, count, 7));
    TEST_ASSERT_TRUE(rankOf(sigs, order, count, 1) < rankOf(sigs, order, count, 17));
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_burst_detector);
    RUN_TEST(test_lr1121_train);
    RUN_TEST(test_lr1121_identical_on_air);
    RUN_TEST(test_lr1121_separability_report);
    RUN_TEST(test_rank_synthetic);
    RUN_TEST(test_sx127x);
    RUN_TEST(test_sx1280);
    RUN_TEST(test_lr2021);
    RUN_TEST(test_nothing_detected_falls_back_to_prior);
    RUN_TEST(test_empty_band_tried_last);
    RUN_TEST(test_clutter_does_not_sink_truth);
    RUN_TEST(test_bench_capture_2g4_100hz_8ch);
    RUN_TEST(test_bench_capture_2g4_100hz_8ch_second);
    RUN_TEST(test_bench_capture_2g4_fsk_dvda);
    RUN_TEST(test_bench_capture_sx1280_333hz_8ch);
    RUN_TEST(test_clutter_alone_rarely_matches_clearly);
    RUN_TEST(test_leakage_does_not_rule_out_two_per_hop);
    RUN_TEST(test_silent_band_rules_out_two_per_hop);
    RUN_TEST(test_cheap_weak_penalty_before_expensive_unknown);
    UNITY_END();

    return 0;
}
