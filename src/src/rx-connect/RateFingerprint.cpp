#include "RateFingerprint.h"

#include <cmath>

namespace RateFingerprint
{

RateSignature makeSignature(uint8_t rateIndex, const uint32_t airtimeUs[SLOT_COUNT],
                            uint32_t intervalUs, uint8_t hopInterval, const uint8_t channelCount[SLOT_COUNT])
{
    RateSignature sig = {};
    sig.rateIndex = rateIndex;
    sig.intervalUs = intervalUs;
    sig.hopInterval = hopInterval;
    sig.trainUs = intervalUs * hopInterval;
    for (uint8_t slot = 0; slot < SLOT_COUNT; slot++)
    {
        sig.onBand[slot] = airtimeUs[slot] != 0;
        sig.airtimeUs[slot] = airtimeUs[slot];
        if (sig.onBand[slot])
            sig.revisitUs[slot] = sig.trainUs * channelCount[slot];
    }
    return sig;
}

BurstDetector::BurstDetector(Burst *out, uint16_t capacity, int8_t threshold, uint8_t hysteresisDb,
                             uint32_t mergeGapUs, uint32_t minBurstUs)
    : out(out), capacity(capacity), threshold(threshold), lowThreshold(threshold - hysteresisDb),
      mergeGapUs(mergeGapUs), minBurstUs(minBurstUs)
{
}

void BurstDetector::endBurst(uint32_t endUs, bool clipped)
{
    out[found].endUs = endUs;
    // Bursts touching the window edges are clipped, so keep them whatever their length
    if (clipped || out[found].startUs == 0 || endUs - out[found].startUs >= minBurstUs)
        ++found;
    else
        ++short_;
    inBurst = false;
}

void BurstDetector::sample(uint32_t nowUs, int8_t rssi)
{
    const bool firstSample = first;
    first = false;

    if (!inBurst)
    {
        if (rssi >= threshold && found < capacity)
        {
            out[found].startUs = firstSample ? 0 : nowUs;
            inBurst = true;
            below = false;
        }
    }
    else if (rssi < lowThreshold)
    {
        // Only end the burst once it has stayed low for mergeGapUs
        if (!below)
        {
            below = true;
            belowSinceUs = nowUs;
        }
        else if (nowUs - belowSinceUs >= mergeGapUs)
        {
            endBurst(belowSinceUs, false);
        }
    }
    else
    {
        below = false;
    }
}

uint32_t BurstDetector::dropOldest(uint16_t n)
{
    if (n > found)
        n = found;
    if (n == 0)
        return 0;
    const uint32_t newStartUs = out[n - 1].endUs;
    // Move the complete bursts, and the start of any open one
    const uint16_t move = found - n + (inBurst ? 1 : 0);
    for (uint16_t i = 0; i < move; i++)
        out[i] = out[i + n];
    found -= n;
    return newStartUs;
}

uint16_t BurstDetector::finish(uint32_t windowUs)
{
    if (inBurst)
    {
        if (below)
            endBurst(belowSinceUs, false);
        else
            endBurst(windowUs, true);
    }
    return found;
}

namespace
{

float maxf(float a, float b) { return a > b ? a : b; }

// Unnormalised Gaussian, 1 for a perfect match
float gauss(float residual, float sigma)
{
    const float z = residual / sigma;
    return expf(-0.5f * z * z);
}

// Log likelihood ratio of an observation being this rate's signal rather than background clutter.
// About 0 for anything that doesn't match, so clutter costs nothing, up to log(1 + signalToClutter)
float evidence(float match, const Tuning &tuning)
{
    return logf(1.0f + tuning.signalToClutter * match);
}

struct Train
{
    uint32_t startUs;   // first packet seen, which may not be the train's first slot
    float evidence;
    uint8_t matched;
};

// The trains found under one rate's hypothesis. The TX sends on a fixed timer, so every real train
// sits on the same grid of packet intervals, while clutter that happens to look like a train doesn't.
class TrainList
{
public:
    // Keep the strongest MAX_TRAINS
    void add(const Train &train)
    {
        if (count < MAX_TRAINS)
        {
            trains[count++] = train;
            return;
        }
        uint8_t weakest = 0;
        for (uint8_t i = 1; i < count; i++)
        {
            if (trains[i].evidence < trains[weakest].evidence)
                weakest = i;
        }
        if (train.evidence > trains[weakest].evidence)
            trains[weakest] = train;
    }

    /**
     * Sum the strongest evidenceCap trains on the same grid as the strongest train, so a rate seen
     * many times (a fast rate, or one leaking in from neighbouring channels) doesn't swamp one seen
     * once. Needs two aligned trains, or one with at least 3 packets, as clutter readily makes a
     * single pair of the right length and spacing.
     */
    float score(const RateSignature &sig, float intervalSigma, const Tuning &tuning) const
    {
        if (count == 0)
            return 0;
        uint8_t best = 0;
        for (uint8_t i = 1; i < count; i++)
        {
            if (trains[i].evidence > trains[best].evidence)
                best = i;
        }

        float aligned[MAX_TRAINS];
        uint8_t alignedCount = 0;
        for (uint8_t i = 0; i < count; i++)
        {
            // Trains start on the hop grid. With two packets per hop a pair fills a whole train, so it
            // must sit on that grid; longer trains may be missing their first packet (telemetry)
            const uint32_t grid = sig.hopInterval <= 2 ? sig.trainUs : sig.intervalUs;
            const uint32_t a = trains[i].startUs, b = trains[best].startUs;
            const uint32_t offset = (a > b ? a - b : b - a) % grid;
            const uint32_t distance = offset < grid - offset ? offset : grid - offset;
            if (distance <= tuning.matchSigmas * intervalSigma)
                aligned[alignedCount++] = trains[i].evidence;
        }
        // A single train is only enough with 3 or more packets, so never for 2 packets per hop
        if (alignedCount < 2 && (trains[best].matched < 3 || sig.hopInterval <= 2))
            return 0;

        // Strongest first
        for (uint8_t i = 1; i < alignedCount; i++)
        {
            for (uint8_t j = i; j > 0 && aligned[j] > aligned[j - 1]; j--)
            {
                const float t = aligned[j];
                aligned[j] = aligned[j - 1];
                aligned[j - 1] = t;
            }
        }
        float total = 0;
        for (uint8_t i = 0; i < alignedCount && i < tuning.evidenceCap; i++)
            total += aligned[i];
        return total;
    }

private:
    static constexpr uint8_t MAX_TRAINS = 16;
    Train trains[MAX_TRAINS];
    uint8_t count = 0;
};

float scoreBand(const RateSignature &sig, uint8_t slot, const BandCapture &capture, const Tuning &tuning)
{
    TrainList trains;

    const uint32_t expectedAirtime = sig.airtimeUs[slot];
    const float airtimeSigma = maxf(tuning.airtimeSigmaFrac * expectedAirtime, tuning.resolutionUs);
    const float intervalSigma = maxf(maxf(tuning.intervalSigmaFrac * sig.intervalUs, tuning.resolutionUs), tuning.edgeJitterUs);
    const float maxAirtime = expectedAirtime + tuning.rssiTailUs + tuning.matchSigmas * airtimeSigma;

    // Evidence for the train being built. Bursts only count once they are paired with another at
    // the right spacing, so clutter of the right length on its own isn't evidence
    const Burst *prev = nullptr;
    const Burst *trainFirst = nullptr;
    float trainEvidence = 0;
    uint8_t trainMatched = 0;   // packets in the train paired at the right spacing
    float prevAirtime = 0;      // airtime evidence of prev, not yet counted
    bool prevCounted = false;

    for (size_t i = 0; i <= capture.count; ++i)
    {
        // Walk the bursts that could be this rate's packets; bursts with the wrong length are clutter
        const Burst *burst = nullptr;
        float burstAirtime = 0;
        if (i < capture.count)
        {
            const Burst &b = capture.bursts[i];
            const float measured = b.endUs - b.startUs;
            // Bursts clipped by the window edges don't show their true length, but may be shorter
            const bool clipped = b.startUs <= capture.startUs || b.endUs >= capture.endUs;
            if (expectedAirtime == 0)
            {
                burst = &b;
            }
            else if (clipped)
            {
                if (measured <= maxAirtime)
                    burst = &b;
            }
            else
            {
                // RSSI decays slowly after a packet ends, so a burst measures up to rssiTailUs longer
                float residual = measured - expectedAirtime;
                if (residual > tuning.rssiTailUs)
                    residual -= tuning.rssiTailUs;
                else if (residual > 0)
                    residual = 0;
                if (fabsf(residual) <= tuning.matchSigmas * airtimeSigma)
                {
                    burst = &b;
                    burstAirtime = evidence(gauss(residual, airtimeSigma), tuning);
                }
            }
            if (burst == nullptr)
                continue;
        }

        // A train is a run of packets each starting within one train length of the previous; the
        // next visit to the sync channel is several trains later
        const bool sameTrain = burst && prev && burst->startUs - prev->startUs <= sig.trainUs;
        bool counted = false;
        if (sameTrain)
        {
            // Start to start spacing should be a whole number of intervals, more than one if packets
            // were missed. Using starts cancels any bias in the RSSI threshold.
            const uint32_t delta = burst->startUs - prev->startUs;
            const uint32_t slots = (delta + sig.intervalUs / 2) / sig.intervalUs;
            const float residual = (float)delta - (float)slots * sig.intervalUs;
            // Packets past the rate's count per hop are leakage from neighbouring channels, which
            // shows the grid but must not let one long run (or clutter) score like several trains
            if (slots >= 1 && slots < sig.hopInterval && trainMatched < sig.hopInterval
                && fabsf(residual) <= tuning.matchSigmas * intervalSigma)
            {
                trainEvidence += evidence(gauss(residual, intervalSigma), tuning) + burstAirtime;
                if (!prevCounted)
                {
                    trainEvidence += prevAirtime;
                    ++trainMatched;
                }
                ++trainMatched;
                counted = true;
            }
        }
        else if (trainFirst)
        {
            // The previous train ended. Judge its packet count if the whole train could have fitted
            // in the window
            const bool complete = trainFirst->startUs >= capture.startUs + sig.trainUs
                && prev->endUs + sig.trainUs <= capture.endUs;
            const uint32_t span = prev->startUs - trainFirst->startUs;
            const int32_t packets = (span + sig.intervalUs / 2) / sig.intervalUs + 1;
            const int32_t missing = (int32_t)sig.hopInterval - packets;
            // More packets than this rate sends per hop still counts: with a strong signal, packets on
            // neighbouring channels leak through and run on from the sync channel's train
            if (trainEvidence > 0)
            {
                if (complete && missing == 0)
                    trainEvidence += tuning.trainMatch;
                else if (complete && missing == 1)
                    trainEvidence += tuning.trainMatch / 2; // e.g. a telemetry slot
                trains.add({trainFirst->startUs, trainEvidence, trainMatched});
            }
        }

        if (!sameTrain)
        {
            trainFirst = burst;
            trainEvidence = 0;
            trainMatched = 0;
        }
        prev = burst;
        prevAirtime = burstAirtime;
        prevCounted = counted;
    }

    return trains.score(sig, intervalSigma, tuning);
}

} // namespace

float scoreLikelihood(const RateSignature &sig, const BandCapture captures[SLOT_COUNT], const Tuning &tuning)
{
    // Evidence adds up across bands, so a dual band rate seen on both beats a single band rate
    // matching only one of them
    float total = 0;
    for (uint8_t slot = 0; slot < SLOT_COUNT; slot++)
    {
        if (!sig.onBand[slot] || !captures[slot].valid)
            continue;
        const float band = scoreBand(sig, slot, captures[slot], tuning);
        if (band > 0)
        {
            total += band;
        }
        else if (sig.hopInterval <= 2 && captures[slot].count > 0)
        {
            // Two packets per hop only show as pairs on the sync channel, which clutter, or the TX's
            // own packets leaking in from its other channels when it's close, easily hide. So not
            // matching isn't evidence the rate is absent, only a silent band is
        }
        else
        {
            // Looked and saw nothing: weakly less likely than a band not yet listened to, in
            // proportion to the chance a train should have been seen in the window
            const float window = captures[slot].endUs - captures[slot].startUs;
            const float seen = sig.revisitUs[slot] ? window / sig.revisitUs[slot] : 1;
            total += band - tuning.absencePenalty * (seen < 1 ? seen : 1);
        }
    }
    return total;
}

// What trying the rate costs, in log seconds: its dwell in the scan is about the time between visits
// to the sync channel, the longest of its bands
static float logCost(const RateSignature &sig)
{
    uint32_t cost = 1000;
    for (uint8_t slot = 0; slot < SLOT_COUNT; slot++)
    {
        if (sig.onBand[slot] && sig.revisitUs[slot] > cost)
            cost = sig.revisitUs[slot];
    }
    return logf(cost / 1e6f);
}

size_t rankRates(const RateSignature *sigs, const bool *candidate, size_t count,
                 const BandCapture captures[SLOT_COUNT], int lastConnected, const Tuning &tuning,
                 uint8_t *orderOut, float *scoresOut)
{
    // Rates with evidence come first, best first. The rest are sorted quickest to find first, which is
    // probability over cost: the scores are log likelihood ratios, so by score - log(cost). Rates on a
    // band heard to be silent come last. Rate tables are small, so these live on the stack and a
    // stable insertion sort is enough
    float scores[MAX_RATES];
    float keys[MAX_RATES];
    uint8_t tiers[MAX_RATES];
    size_t ranked = 0;
    for (size_t i = 0; i < count && i < MAX_RATES; ++i)
    {
        if (!candidate[i])
            continue;
        scores[i] = scoreLikelihood(sigs[i], captures, tuning);
        if ((int)i == lastConnected)
            scores[i] += tuning.priorBoost;
        if (scoresOut)
            scoresOut[i] = scores[i];
        bool silent = false;
        for (uint8_t slot = 0; slot < SLOT_COUNT; slot++)
            silent |= sigs[i].onBand[slot] && captures[slot].valid && captures[slot].count == 0;
        tiers[i] = scores[i] > 0 ? 0 : silent ? 2 : 1;
        keys[i] = tiers[i] == 0 ? scores[i] : scores[i] - logCost(sigs[i]);

        size_t pos = ranked++;
        while (pos > 0 && (tiers[orderOut[pos - 1]] > tiers[i]
            || (tiers[orderOut[pos - 1]] == tiers[i] && keys[orderOut[pos - 1]] < keys[i])))
        {
            orderOut[pos] = orderOut[pos - 1];
            --pos;
        }
        orderOut[pos] = i;
    }
    return ranked;
}

} // namespace RateFingerprint
