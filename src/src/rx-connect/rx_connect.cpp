#include "rx_connect.h"

/**
 * The RX's search for the TX's packet rate while disconnected.
 *
 * While waiting for a SYNC on the current rate, the main loop keeps polling the RSSI on the sync
 * channel the radio is parked on and turns it into bursts. RateFingerprint scores how well each
 * rate's on-air signature fits them. When the scan moves on, the untried rate that fits best is
 * next, and it moves on early if another rate clearly fits and the current one shows nothing.
 * DEBUG_RATE_FINGERPRINT prints the captures and rankings.
 */

#include "FHSS.h"
#include "LQCALC.h"
#include "RateFingerprint.h"
#include "config.h"
#include "logging.h"
#include "options.h"

using namespace RateFingerprint;

// From rx_main.cpp
extern uint32_t LastSyncPacket;
extern LQCALC<100> LQCalc;
extern LQCALC<100> LQCalcDVDA;
void SetRFLinkRate(uint8_t index, bool bindMode);

// Burst threshold above the noise floor
#if !defined(RATE_FINGERPRINT_MARGIN_DB)
#define RATE_FINGERPRINT_MARGIN_DB 10
#endif
#if !defined(RATE_FINGERPRINT_MARGIN_DB_GFSK)
#define RATE_FINGERPRINT_MARGIN_DB_GFSK 14
#endif
// Leave the current rate early for an untried one scoring at least this (shown x10 in the debug
// output, so 200) while the current rate scores nothing. 0 disables leaving early.
#if !defined(RATE_FINGERPRINT_LEAVE_SCORE)
#define RATE_FINGERPRINT_LEAVE_SCORE 20
#endif

/////////////////////////////////////////////////////////////////////////////////////////////////
// RSSI capture

#if defined(PLATFORM_ESP8266)
static constexpr uint16_t MAX_BURSTS = 128;
#else
static constexpr uint16_t MAX_BURSTS = 256;
#endif
static constexpr uint8_t PRINT_BURSTS = 16;
static constexpr uint8_t HYSTERESIS_DB = 3;
// The shortest packet is ~450us and the shortest gap between packets ~260us
static constexpr uint32_t MIN_BURST_US = 200;
static constexpr uint32_t MERGE_GAP_US = 100;
// Samples taken at the start of each capture to find the noise floor
static constexpr uint16_t FLOOR_SAMPLES = 200;
// A live capture shorter than this is not used yet, the band's previous capture is instead
static constexpr uint32_t MIN_LIVE_US = 300000;
static constexpr uint32_t EARLY_CHECK_MS = 250;
// No packet is longer than ~30ms (25Hz SF9 plus the RSSI tail), so a longer burst means a strong
// signal never drops below the threshold between packets, e.g. the TX close by leaking in from its
// other channels. Raise the threshold in steps until the gaps show, up to a margin below the peak.
static constexpr uint32_t OVERLONG_BURST_US = 40000;
static constexpr int8_t THRESHOLD_STEP_DB = 6;
static constexpr int8_t THRESHOLD_BELOW_PEAK_DB = 6;
// A band's last capture stops counting after this long, so seeing nothing there doesn't steer the scan
// away from that band for good, e.g. while the TX is still powering up
static constexpr uint32_t STALE_CAPTURE_MS = 5000;

// The capture in progress, on the band of the current rate
static struct
{
    bool active;
    uint8_t slot;
    Burst bursts[MAX_BURSTS];
    BurstDetector detector;
    uint16_t floorSamples;
    uint16_t histogram[129];
    int8_t noiseFloor;
    int8_t threshold;
    int8_t peak;
    uint32_t startMicros;   // time 0 for the burst timestamps
    uint32_t windowStartUs; // moves on as the oldest bursts are discarded
    uint32_t lastUs;
    uint32_t samples;
    uint32_t lastRaiseUs;   // when the threshold was last raised
} live;

// The last finished capture of each band. Single band chips only need one buffer.
#if defined(RADIO_LR1121) || defined(RADIO_LR2021)
static constexpr uint8_t BAND_BUFFERS = SLOT_COUNT;
#else
static constexpr uint8_t BAND_BUFFERS = 1;
#endif
static Burst doneBursts[BAND_BUFFERS][MAX_BURSTS];
static BandCapture done[SLOT_COUNT];
static uint32_t donePollUs[SLOT_COUNT];
static uint32_t doneMillis[SLOT_COUNT];

static uint32_t lastEarlyCheckMs;

#if defined(DEBUG_RATE_FINGERPRINT)
static const char *slotName(uint8_t slot)
{
    return slot == SLOT_2G4 ? "2G4" : "900";
}
#endif

static uint8_t slotFor(const expresslrs_mod_settings_s *modParams)
{
    // Radio 1 is on the 900MHz sync channel for dual band rates
    return RadioBandMod::isB2G4(modParams->radio_type) ? SLOT_2G4 : SLOT_900;
}

static int8_t readRssi()
{
    // The DIO ISR also talks to the radio over SPI, so hold it off until the read is done
    noInterrupts();
#if defined(RADIO_SX127X)
    const int8_t rssi = Radio.GetCurrRSSI(SX12XX_Radio_1);
#elif defined(RADIO_SX128X)
    const int8_t rssi = Radio.GetRssiInst(SX12XX_Radio_1);
#else
    Radio.StartRssiInst(SX12XX_Radio_1);
    const int8_t rssi = Radio.GetRssiInst(SX12XX_Radio_1);
#endif
    interrupts();
    return rssi;
}

static uint32_t livePollUs()
{
    return live.samples ? live.lastUs / live.samples : 1000;
}

static bool liveUsable()
{
    return live.active && live.floorSamples == FLOOR_SAMPLES && live.lastUs - live.windowStartUs >= MIN_LIVE_US;
}

static BandCapture liveCapture()
{
    return {true, live.windowStartUs, live.lastUs, live.bursts, live.detector.count()};
}

// The captures to score with: the live one once long enough, otherwise each band's last one if recent
static void currentCaptures(BandCapture captures[SLOT_COUNT], uint32_t &resolutionUs)
{
    resolutionUs = 0;
    for (uint8_t slot = 0; slot < SLOT_COUNT; slot++)
    {
        if (liveUsable() && live.slot == slot)
        {
            captures[slot] = liveCapture();
            resolutionUs = std::max(resolutionUs, livePollUs());
        }
        else if (done[slot].valid && millis() - doneMillis[slot] < STALE_CAPTURE_MS)
        {
            captures[slot] = done[slot];
            resolutionUs = std::max(resolutionUs, donePollUs[slot]);
        }
        else
        {
            captures[slot] = BandCapture{};
        }
    }
    if (resolutionUs == 0)
        resolutionUs = Tuning().resolutionUs;
}

// Start a new capture on the current rate's sync channel, after the rate changes
static void captureStart()
{
    // Keep what was captured on the previous rate's band, if it ran long enough to be useful
    if (liveUsable())
    {
        const uint16_t count = live.detector.finish(live.lastUs);
        Burst *const buffer = doneBursts[live.slot % BAND_BUFFERS];
        memcpy(buffer, live.bursts, count * sizeof(Burst));
        done[live.slot] = {true, live.windowStartUs, live.lastUs, buffer, count};
        donePollUs[live.slot] = livePollUs();
        doneMillis[live.slot] = millis();
    }

    live.active = true;
    live.slot = slotFor(ExpressLRS_currAirRate_Modparams);
    live.floorSamples = 0;
    memset(live.histogram, 0, sizeof(live.histogram));
    live.peak = -128;
    live.windowStartUs = 0;
    live.lastUs = 0;
    live.samples = 0;
    live.lastRaiseUs = 0;
    lastEarlyCheckMs = millis();
}

// Take one RSSI sample, from the main loop while disconnected and in RX
static void capturePoll()
{
    if (!live.active)
        return;

    const int8_t rssi = readRssi();
    const uint32_t nowMicros = micros();

    if (live.floorSamples < FLOOR_SAMPLES)
    {
        // Noise floor as the 20th percentile of the first samples, which ignores the odd burst
        ++live.histogram[-(int16_t)rssi];
        if (++live.floorSamples < FLOOR_SAMPLES)
            return;
        uint16_t seen = 0;
        int16_t level = 128;
        for (; level > 0; level--)
        {
            seen += live.histogram[level];
            if (seen >= FLOOR_SAMPLES / 5)
                break;
        }
        live.noiseFloor = -level;
        // RSSI is noisier with the radio in GFSK (and presumably FLRC) mode
        const bool fsk = !RadioBandMod::isLoRa(ExpressLRS_currAirRate_Modparams->radio_type);
        live.threshold = live.noiseFloor + (fsk ? RATE_FINGERPRINT_MARGIN_DB_GFSK : RATE_FINGERPRINT_MARGIN_DB);
        live.detector = BurstDetector(live.bursts, MAX_BURSTS, live.threshold, HYSTERESIS_DB, MERGE_GAP_US, MIN_BURST_US);
        live.startMicros = nowMicros;
        return;
    }

    live.lastUs = nowMicros - live.startMicros;
    ++live.samples;
    if (rssi > live.peak)
        live.peak = rssi;
    live.detector.sample(live.lastUs, rssi);

    if (live.detector.openBurstUs(live.lastUs) > OVERLONG_BURST_US && live.lastUs - live.lastRaiseUs > OVERLONG_BURST_US)
    {
        live.lastRaiseUs = live.lastUs;
        const int8_t raised = std::min<int>(live.detector.getThreshold() + THRESHOLD_STEP_DB, live.peak - THRESHOLD_BELOW_PEAK_DB);
        if (raised > live.detector.getThreshold())
        {
            live.detector.setThreshold(raised);
            live.threshold = raised;
        }
    }

    // Slide along, discarding the oldest quarter of the bursts
    if (live.detector.full())
        live.windowStartUs = live.detector.dropOldest(MAX_BURSTS / 4);
}

/////////////////////////////////////////////////////////////////////////////////////////////////
// Rates tried this pass

static bool rateTried[RATE_MAX];
// Tried rates are offered again, once per pass, if their packets later clearly show, e.g. the TX
// was turned on after the rate was tried. Only once, so another pilot's TX can't hold the scan.
static bool rateRetried[RATE_MAX];

// Rates with the same radio config are covered by one visit, as the SYNC carries the actual rate
static void markRateTried(uint8_t index)
{
    for (uint8_t i = 0; i < RATE_MAX; i++)
    {
        if (isSameRadioConfig(i, index))
        {
            // A rate visited again is a retry
            if (rateTried[i])
                rateRetried[i] = true;
            rateTried[i] = true;
        }
    }
}

static void clearRatesTried()
{
    memset(rateTried, 0, sizeof(rateTried));
    memset(rateRetried, 0, sizeof(rateRetried));
}

/////////////////////////////////////////////////////////////////////////////////////////////////
// Ranking the rates on what has been captured

static RateSignature signatureFor(uint8_t index)
{
    const expresslrs_mod_settings_s *const m = get_elrs_airRateConfig(index);
    const expresslrs_rf_pref_params_s *const rfPerf = get_elrs_RFperfParams(index);
    uint32_t airtimeUs[SLOT_COUNT] = {};
    uint8_t channelCount[SLOT_COUNT] = {};
#if defined(RADIO_LR1121) || defined(RADIO_LR2021)
    channelCount[SLOT_900] = FHSSconfig->freq_count;
    channelCount[SLOT_2G4] = FHSSconfigDualBand->freq_count;
    if (RadioBandMod::isBDUAL(m->radio_type))
    {
        // TOA is the 900MHz radio's packet, TOA2 the 2.4GHz radio's
        airtimeUs[SLOT_900] = rfPerf->TOA;
        airtimeUs[SLOT_2G4] = rfPerf->TOA2;
        return makeSignature(index, airtimeUs, m->interval, m->FHSShopInterval, channelCount);
    }
#else
    // The primary domain is the radio's only band on single band chips
    channelCount[slotFor(m)] = FHSSconfig->freq_count;
#endif
    airtimeUs[slotFor(m)] = rfPerf->TOA;
    return makeSignature(index, airtimeUs, m->interval, m->FHSShopInterval, channelCount);
}

#if defined(DEBUG_RATE_FINGERPRINT)
static void printCapture(uint8_t slot, const BandCapture &capture, bool isLive)
{
    if (!capture.valid)
    {
        DBGLN("FP capture %s: none or stale", slotName(slot));
        return;
    }
    DBGLN("FP capture %s %s %ums: %uus per poll, %u bursts", slotName(slot), isLive ? "live" : "last",
        (capture.endUs - capture.startUs) / 1000, isLive ? livePollUs() : donePollUs[slot], capture.count);
    if (isLive)
    {
        DBGLN("FP   floor %d thr %d peak %d, %u short dropped", live.noiseFloor, live.threshold, live.peak, live.detector.dropped());
        // The most recent bursts
        const uint16_t first = capture.count > PRINT_BURSTS ? capture.count - PRINT_BURSTS : 0;
        for (uint16_t i = first; i < capture.count; i++)
        {
            const Burst &b = capture.bursts[i];
            DBGLN("FP   burst @%uus len %uus gap %uus", b.startUs, b.endUs - b.startUs,
                i ? b.startUs - capture.bursts[i - 1].endUs : 0);
        }
    }
}

static void printRanking(const char *why, const RateSignature sigs[RATE_MAX], const BandCapture captures[SLOT_COUNT],
                         const uint8_t order[RATE_MAX], const float scores[RATE_MAX], size_t count)
{
    DBGLN("FP %s", why);
    for (uint8_t slot = 0; slot < SLOT_COUNT; slot++)
        printCapture(slot, captures[slot], captures[slot].bursts == live.bursts);
    DBGLN("FP rank of untried (score x10):");
    for (size_t i = 0; i < count; i++)
    {
        const RateSignature &sig = sigs[order[i]];
        const char *band = sig.onBand[SLOT_900] && sig.onBand[SLOT_2G4] ? "dual" : sig.onBand[SLOT_2G4] ? "2G4" : "900";
        DBGLN("FP %u. rate %u %s %uus hop %u airtime %u/%uus: %d", i + 1, order[i], band, sig.intervalUs,
            sig.hopInterval, sig.airtimeUs[SLOT_900], sig.airtimeUs[SLOT_2G4], (int32_t)(scores[order[i]] * 10));
    }
}
#endif

static size_t rank(const bool tried[RATE_MAX], RateSignature sigs[RATE_MAX], BandCapture captures[SLOT_COUNT],
                   Tuning &tuning, uint8_t order[RATE_MAX], float scores[RATE_MAX])
{
    static_assert(RATE_MAX <= MAX_RATES, "RateFingerprint rate limit");
    bool candidate[RATE_MAX];
    for (uint8_t i = 0; i < RATE_MAX; i++)
    {
        sigs[i] = signatureFor(i);
        candidate[i] = isSupportedRFRate(i) && !tried[i];
    }
    currentCaptures(captures, tuning.resolutionUs);
    // The saved rate is always tried first, so it needs no prior here
    return rankRates(sigs, candidate, RATE_MAX, captures, -1, tuning, order, scores);
}

// The tried rate (other than current) whose packets now clearly show, scoring more than score, or
// RATE_MAX. score is updated to its score.
static uint8_t retryRate(const BandCapture captures[SLOT_COUNT], const Tuning &tuning, uint8_t current, float &score)
{
    if (RATE_FINGERPRINT_LEAVE_SCORE == 0)
        return RATE_MAX;
    uint8_t best = RATE_MAX;
    for (uint8_t i = 0; i < RATE_MAX; i++)
    {
        if (!isSupportedRFRate(i) || !rateTried[i] || rateRetried[i] || isSameRadioConfig(i, current))
            continue;
        const float retryScore = scoreLikelihood(signatureFor(i), captures, tuning);
        if (retryScore >= RATE_FINGERPRINT_LEAVE_SCORE && retryScore > score)
        {
            best = i;
            score = retryScore;
        }
    }
    return best;
}

// The best untried supported rate, or a tried one if its packets now clearly show, or RATE_MAX if
// every rate has been tried
static uint8_t bestNextRate(uint8_t current)
{
    RateSignature sigs[RATE_MAX];
    BandCapture captures[SLOT_COUNT];
    Tuning tuning;
    uint8_t order[RATE_MAX];
    float scores[RATE_MAX];
    const size_t count = rank(rateTried, sigs, captures, tuning, order, scores);
    float bestScore = count ? scores[order[0]] : -1e9f;
    const uint8_t retry = retryRate(captures, tuning, current, bestScore);
#if defined(DEBUG_RATE_FINGERPRINT)
    printRanking("dwell over", sigs, captures, order, scores, count);
    if (retry != RATE_MAX)
        DBGLN("FP retrying rate %u: %d", retry, (int32_t)(bestScore * 10));
#endif
    if (retry != RATE_MAX)
        return retry;
    return count ? order[0] : RATE_MAX;
}

// True if the rate's packets show in the capture, or it is too early to tell
static bool rateSeen(uint8_t index)
{
    if (!liveUsable())
        return true;

    BandCapture captures[SLOT_COUNT];
    Tuning tuning;
    currentCaptures(captures, tuning.resolutionUs);
    const bool seen = scoreLikelihood(signatureFor(index), captures, tuning) > 0;
#if defined(DEBUG_RATE_FINGERPRINT)
    if (!seen)
        DBGLN("FP rate %u not seen, ending the extended dwell", index);
#endif
    return seen;
}

// A rate to move to now as it clearly fits and the current rate doesn't, or RATE_MAX to stay
static uint8_t earlyRate(uint8_t current)
{
    if (RATE_FINGERPRINT_LEAVE_SCORE == 0 || !liveUsable())
        return RATE_MAX;
    const uint32_t now = millis();
    if (now - lastEarlyCheckMs < EARLY_CHECK_MS)
        return RATE_MAX;
    lastEarlyCheckMs = now;

    RateSignature sigs[RATE_MAX];
    BandCapture captures[SLOT_COUNT];
    Tuning tuning;
    uint8_t order[RATE_MAX];
    float scores[RATE_MAX];
    const size_t count = rank(rateTried, sigs, captures, tuning, order, scores);
    float bestScore = count ? scores[order[0]] : -1e9f;
    uint8_t best = count ? order[0] : RATE_MAX;
    const uint8_t retry = retryRate(captures, tuning, current, bestScore);
    if (retry != RATE_MAX)
        best = retry;
    if (best == RATE_MAX || bestScore < RATE_FINGERPRINT_LEAVE_SCORE)
        return RATE_MAX;
    // If the TX were on the current rate it would show up, so only leave if it doesn't
    if (scoreLikelihood(signatureFor(current), captures, tuning) > 0)
        return RATE_MAX;

#if defined(DEBUG_RATE_FINGERPRINT)
    printRanking("leaving early", sigs, captures, order, scores, count);
    if (retry != RATE_MAX)
        DBGLN("FP retrying rate %u: %d", retry, (int32_t)(bestScore * 10));
#endif
    return best;
}

/////////////////////////////////////////////////////////////////////////////////////////////////
// The scan

// Dwell multiplier after receiving a packet on the rate, or (halved) for the saved rate at boot
static constexpr uint8_t RFmodeCycleMultiplierSlow = 10;

static uint32_t cycleInterval; // dwell on the current rate in ms, before any multiplier
static uint32_t RFmodeLastCycled;
static uint8_t RFmodeCycleMultiplier;
static bool LockRFmode;

static uint8_t savedRate()
{
    uint8_t rate = config.GetRateInitialIdx();
    for (uint8_t i = 0; i < RATE_MAX && !isSupportedRFRate(rate); i++)
        rate = (rate + 1) % RATE_MAX;
    return rate;
}

static uint8_t nextScanRate()
{
    // A pass starts with the saved rate
    bool anyTried = false;
    for (uint8_t i = 0; i < RATE_MAX; i++)
        anyTried |= rateTried[i];
    uint8_t next = anyTried ? bestNextRate(ExpressLRS_currAirRate_Modparams->index) : savedRate();
    if (next == RATE_MAX)
    {
        // Every rate has been tried, start a new pass
        next = savedRate();
        DBGLN("Rate scan pass complete, restarting at %u", next);
        clearRatesTried();
    }
    markRateTried(next);
    return next;
}

static void switchScanRate(uint8_t next)
{
    RFmodeLastCycled = millis();
    LastSyncPacket = RFmodeLastCycled;
    SetRFLinkRate(next, false);
    LQCalc.reset100();
    LQCalcDVDA.reset100();
    Radio.RXnb();
    captureStart();
    DBGLN("%u", ExpressLRS_currAirRate_Modparams->interval);
}

void rxConnectBegin()
{
    const uint8_t initialRate = savedRate();
    SetRFLinkRate(initialRate, false);
    clearRatesTried();
    markRateTried(initialRate);
    captureStart();
    // Start slow on the selected rate to give it the best chance
    // to connect before beginning rate cycling
    RFmodeCycleMultiplier = RFmodeCycleMultiplierSlow / 2;
}

void rxConnectRateChanged(uint8_t index, uint32_t intervalUs)
{
    // Wait for (11/10) 110% of time it takes to cycle through all freqs in FHSS table (in ms).
    // The scan only visits one rate of each group with the same radio config, so wait for the
    // slowest rate in the group
    const expresslrs_mod_settings_s *const modParams = get_elrs_airRateConfig(index);
    cycleInterval = 0;
    for (uint8_t i = 0; i < RATE_MAX; i++)
    {
        if (isSameRadioConfig(i, index))
        {
            const expresslrs_mod_settings_s *const groupParams = get_elrs_airRateConfig(i);
            const uint32_t groupInterval = groupParams->interval * intervalUs / modParams->interval;
            const uint32_t groupCycleInterval = ((uint32_t)11U * FHSSgetChannelCount() * groupParams->FHSShopInterval * groupInterval) / (10U * 1000U);
            cycleInterval = std::max(cycleInterval, groupCycleInterval);
        }
    }
}

void rxConnectRestartDwell(unsigned long now)
{
    RFmodeLastCycled = now;
}

void ICACHE_RAM_ATTR rxConnectTentative(unsigned long now)
{
    RFmodeLastCycled = now; // give another 3 sec for lock to occur
}

void ICACHE_RAM_ATTR rxConnectPacketReceived()
{
    // Extend sync duration since we've received a packet at this rate
    // but do not extend it indefinitely
    RFmodeCycleMultiplier = RFmodeCycleMultiplierSlow;
}

void rxConnectConnected()
{
    LockRFmode = firmwareOptions.lock_on_first_connection;
}

void rxConnectLost(bool wasConnected, uint8_t rate)
{
    // After losing a connection start a new scan pass on the rate that was connected. Otherwise
    // fingerprinting was paused while tentative, so carry on the same pass. Either way this rate
    // (or the one a SYNC just moved us to) counts as tried
    if (wasConnected)
        clearRatesTried();
    // Already tried if the lock failed on the rate we were on, which isn't a retry
    if (!rateTried[rate])
        markRateTried(rate);
    captureStart();
}

void rxConnectRestartScan()
{
    RFmodeLastCycled = 0;
    LockRFmode = false;
    clearRatesTried();
}

void rxConnectUpdate(unsigned long now)
{
    if (connectionState == connected || connectionState == wifiUpdate || InBindingMode)
        return;

    // A SYNC for us has been received on this rate, so stay on it. If the lock doesn't complete,
    // RxLockTimeoutMs drops back to disconnected and the scan carries on from here.
    // With lock_on_first_connection there is no scanning at all after the first connection.
    if (connectionState != disconnected || LockRFmode)
        return;

    // Keep sampling the RSSI while waiting for a SYNC
    capturePoll();

    // A SYNC may have arrived during the RSSI read, which makes us tentative and moves
    // RFmodeLastCycled past the now passed in, so check again and use a fresh now
    if (connectionState != disconnected)
        return;
    now = millis();

    const uint8_t early = earlyRate(ExpressLRS_currAirRate_Modparams->index);
    if (early != RATE_MAX)
    {
        markRateTried(early);
        switchScanRate(early);
        RFmodeCycleMultiplier = 1;
        return;
    }

    // The saved rate after boot gets a longer dwell. Only keep it while that rate's packets can be
    // seen, otherwise move on after a normal dwell so the other rates, and the other band on dual
    // band hardware, get a look sooner. It comes round again on the next pass.
    static uint32_t lastSeenCheckMs;
    if (RFmodeCycleMultiplier > 1 && (int32_t)(now - RFmodeLastCycled) > (int32_t)cycleInterval
        && now - lastSeenCheckMs >= 250)
    {
        lastSeenCheckMs = now;
        if (!rateSeen(ExpressLRS_currAirRate_Modparams->index))
            RFmodeCycleMultiplier = 1;
    }

    // Move on to the next rate once the dwell is over. Signed, as the RX ISR can move
    // RFmodeLastCycled past now when a SYNC arrives
    if ((int32_t)(now - RFmodeLastCycled) > (int32_t)(cycleInterval * RFmodeCycleMultiplier))
    {
        switchScanRate(nextScanRate());

        // Switch to FAST_SYNC if not already in it (won't be if was just connected)
        RFmodeCycleMultiplier = 1;
    }
}
