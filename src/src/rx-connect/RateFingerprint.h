#pragma once

#include <stdint.h>
#include <stddef.h>

/**
 * Rank air rates by how well RSSI bursts captured on the sync channel match each rate's
 * expected on-air signature. Used to order the receiver's disconnected rate scan so the
 * most likely rate is tried first. The ranking only orders the scan, it never removes a rate.
 *
 * All timing is in microseconds. Nothing here touches the radio, so it is chip independent: a
 * rate's signature comes from its ModParams (interval, hop interval) and RFperf (TOA, and TOA2 for
 * the 2.4GHz radio of dual band rates) table entries.
 */
namespace RateFingerprint
{

// Index into the per-band arrays below
enum BandSlot : uint8_t
{
    SLOT_900 = 0,
    SLOT_2G4 = 1,
    SLOT_COUNT
};

struct RateSignature
{
    uint8_t rateIndex;
    bool onBand[SLOT_COUNT];        // dual band rates transmit on both
    uint32_t airtimeUs[SLOT_COUNT]; // single packet on-air time, per band
    uint32_t intervalUs;            // packet interval
    uint8_t hopInterval;            // packets per channel before hopping
    uint32_t trainUs;               // hopInterval * interval, the dwell on any one channel
    uint32_t revisitUs[SLOT_COUNT]; // channelCount * trainUs, time between visits to the sync channel
};

/**
 * @param airtimeUs packet time on air per band, 0 for a band the rate doesn't use
 * @param channelCount number of FHSS channels per band, which sets how often the sync channel is revisited
 */
RateSignature makeSignature(uint8_t rateIndex, const uint32_t airtimeUs[SLOT_COUNT],
                            uint32_t intervalUs, uint8_t hopInterval, const uint8_t channelCount[SLOT_COUNT]);

// A period where the RSSI on the sync channel was above the detection threshold
struct Burst
{
    uint32_t startUs;
    uint32_t endUs;
};

/**
 * Turns a stream of RSSI samples into bursts. Dips shorter than mergeGapUs are ripple within one
 * packet (e.g. a LoRa chirp through a narrower FSK filter), and bursts shorter than minBurstUs are
 * noise, except where clipped by the window edges.
 */
class BurstDetector
{
public:
    BurstDetector() = default;
    BurstDetector(Burst *out, uint16_t capacity, int8_t threshold, uint8_t hysteresisDb,
                  uint32_t mergeGapUs, uint32_t minBurstUs);

    // nowUs is relative to the window start; a burst already high on the first sample starts at 0
    void sample(uint32_t nowUs, int8_t rssi);
    // Closes any open burst at the window end, returns the number of bursts
    uint16_t finish(uint32_t windowUs);

    /**
     * Discard the oldest n complete bursts to make room, for a capture that slides along in time.
     * @return the new window start, the end of the last discarded burst
     */
    uint32_t dropOldest(uint16_t n);

    uint16_t count() const { return found; }
    // How long the burst in progress has been going, 0 if none
    uint32_t openBurstUs(uint32_t nowUs) const { return inBurst ? nowUs - out[found].startUs : 0; }
    int8_t getThreshold() const { return threshold; }
    // Change the threshold, e.g. raise it if a strong signal never drops below it between packets
    void setThreshold(int8_t newThreshold)
    {
        lowThreshold = newThreshold - (threshold - lowThreshold);
        threshold = newThreshold;
    }
    uint16_t dropped() const { return short_; }
    bool full() const { return found == capacity; }

private:
    void endBurst(uint32_t endUs, bool clipped);

    Burst *out = nullptr;
    uint16_t capacity = 0;
    int8_t threshold = 0;
    int8_t lowThreshold = 0;
    uint32_t mergeGapUs = 0;
    uint32_t minBurstUs = 0;

    uint16_t found = 0;
    uint16_t short_ = 0;
    bool first = true;
    bool inBurst = false;
    bool below = false;
    uint32_t belowSinceUs = 0;
};

// Timestamps should be relative to the capture start so they can't wrap
struct BandCapture
{
    bool valid;             // false if this band has not been listened to
    uint32_t startUs;       // capture window
    uint32_t endUs;
    const Burst *bursts;    // in time order, clipped to the window
    uint16_t count;
};

struct Tuning
{
    // Scores are log likelihood ratios against background clutter. Bursts that don't fit a rate
    // score about 0. With nothing detected, only the absence penalty and the prior set the order.
    // Evidence only comes from trains: bursts of the right length at the right spacing. A lone burst
    // of the right length is not evidence, as clutter often has similar lengths.
    float signalToClutter = 20.0f;  // a perfect match scores log(1 + this) per airtime or spacing
    float matchSigmas = 3.0f;       // bursts/spacings further than this from expected are clutter
    float trainMatch = 3.0f;        // a train with exactly the expected packet count, half for one missing
    float priorBoost = 4.0f;        // added to the last connected rate
    float absencePenalty = 1.0f;    // nothing matching seen on a captured band, scaled by window / revisit
    uint8_t evidenceCap = 4;        // only the strongest this many trains on the same packet grid count
    uint32_t resolutionUs = 30;     // burst edge timing resolution, e.g. the RSSI poll period (29us measured on LR1121)
    uint32_t rssiTailUs = 500;      // bursts measure up to this much longer than the packet, as RSSI decays slowly
                                    // (seen on LR1121: +150-400us on 480us GFSK, +100-600us on 7.6ms LoRa)
    uint32_t edgeJitterUs = 40;     // burst start timing jitter, packet starts 1ms apart measure 970-1105us
    float airtimeSigmaFrac = 0.05f;
    float intervalSigmaFrac = 0.01f;
};

// Evidence for the rate in the captures, excluding the prior
float scoreLikelihood(const RateSignature &sig, const BandCapture captures[SLOT_COUNT], const Tuning &tuning);

static constexpr size_t MAX_RATES = 32;

/**
 * Score and sort the candidate rates, best first. Ties keep table order.
 * @param candidate rates to include, e.g. isSupportedRFRate(), indexed by position in sigs
 * @param lastConnected position in sigs of the last connected rate, or -1 if none
 * @param orderOut receives positions into sigs, sized for count entries
 * @param scoresOut optional, receives each rate's total score, indexed by position in sigs
 * @return number of entries written to orderOut. Only the first MAX_RATES rates are considered
 */
size_t rankRates(const RateSignature *sigs, const bool *candidate, size_t count,
                 const BandCapture captures[SLOT_COUNT], int lastConnected, const Tuning &tuning,
                 uint8_t *orderOut, float *scoresOut = nullptr);

} // namespace RateFingerprint
