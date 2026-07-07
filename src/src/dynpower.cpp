
#include <dynpower.h>

#include "OTA.h"
#include "POWERMGNT.h"
#include "config.h"
#include "logging.h"

#if defined(TARGET_TX)
#include <handset.h>
#include <LBT.h>

#include "MeanAccumulator.h"
#include "StdevAccumulator.h"

// LQ-based boost defines
#define DYNPOWER_LQ_BOOST_THRESH_DIFF 20  // If LQ is dropped suddenly for this amount (relative), immediately boost to the max power configured.
#define DYNPOWER_LQ_BOOST_THRESH_MIN  50  // If LQ is below this value (absolute), immediately boost to the max power configured.
#define DYNPOWER_LQ_MOVING_AVG_K      8   // Number of previous values for calculating moving average. Best with power of 2.

// RSSI-based increment defines
#define DYNPOWER_RSSI_CNT 5               // Number of RSSI readings to average (straight average) to make an RSSI-based adjustment
#define DYNPOWER_RSSI_THRESH_UP 15        // RSSI < (Sensitivity+Up) -> raise power
#define DYNPOWER_RSSI_THRESH_DN 21        // RSSI > (Sensitivity+Dn) >- lower power

// SNR-based increment defines
constexpr int8_t SNR1dB = SNR_SCALE(1.0); // SNR 1dB scaled

// Ramp-up aggressiveness, selected via config.GetDynamicPowerRampup() (Lua "Ramp-Up": 0=Normal, 1=Aggressive, 2=Very Aggressive)
// Raising and lowering are adjusted together: raise thresholds move to trigger *sooner* (at better signal) while the
// matching lower thresholds move to require *more* margin before backing off, so the raise/lower gap does not shrink
// as aggressiveness increases (for RSSI and the pre-statistics SNR default it is provably constant; for the
// statistics-derived SNR threshold and the bounded 0-100% LQ gate it shrinks much less than if only the raise side moved).
// A runtime cap is still kept on every raise threshold as defense-in-depth in case these arrays are ever edited independently.
#define DYNPOWER_RAMPUP_LEVEL_MAX 2
static constexpr uint8_t DYNPOWER_RAMPUP_LQ_THRESH_UP[]     = {85, 90, 93};                     // Raise: below this LQ, raise power if RSSI/SNR did nothing this period. Step (+5,+3) is not perfectly linear like the rest -- 93 was chosen over the "linear" 95 to keep a 5-point gap against the Very Aggressive DN threshold (98) instead of narrowing to 3, which was firing too often on ordinary link noise
static constexpr uint8_t DYNPOWER_RAMPUP_LQ_THRESH_DN[]     = {95, 97, 98};                     // Lower: lq_avg must be at least this to permit any RSSI/SNR-based lowering. Steps (+2,+1) are not perfectly linear like the rest -- 98 was chosen over the "linear" 99 to avoid requiring near-perfect LQ before Very Aggressive ever permits lowering
static constexpr int8_t  DYNPOWER_RAMPUP_RSSI_BONUS[]       = {0, 4, 8};                        // Added to BOTH the RSSI raise and RSSI lower thresholds (dBm) -- gap stays fixed at 6dB
static constexpr int8_t  DYNPOWER_RAMPUP_SNR_BONUS_SCALED[] = {0, SNR_SCALE(1), SNR_SCALE(2)};  // Added to BOTH the pre-statistics SNR raise and lower thresholds -- gap stays fixed at the per-rate compiled value
static constexpr int32_t DYNPOWER_RAMPUP_SNR_SIGMA_UP_NUM[] = {13, 10, 7};                      // Raise: /4 sigma-multiple subtracted from the SNR mean (13/4=3.25sigma .. 7/4=1.75sigma)
static constexpr int32_t DYNPOWER_RAMPUP_SNR_SIGMA_DN_NUM[] = {2, 5, 8};                        // Lower: /4 sigma-multiple added to the SNR mean (2/4=0.5sigma .. 8/4=2.0sigma). UP_NUM+DN_NUM==15 at every level, so the total up-to-down span stays a constant 3.75sigma
// Missed-telemetry raise delay is scaled by NUM/DEN of the debounced (2x nominal telemetry period) delay, in linear
// steps of 0.5 periods to match the linear shape of every other ramp-up parameter above:
// Normal=2.0x (full debounce, waits for a 2nd miss), Aggressive=1.5x (reacts partway into the 2nd period),
// Very Aggressive=1.0x (reacts on the 1st miss, every time -- no debounce). No lower-side counterpart exists for this mechanism.
static constexpr uint8_t DYNPOWER_RAMPUP_TLM_NUM[] = {1, 3, 1};
static constexpr uint8_t DYNPOWER_RAMPUP_TLM_DEN[] = {1, 4, 2};

template<uint8_t K, uint8_t SHIFT>
class MovingAvg
{
public:
  void init(uint32_t v) { _shiftedVal = v << SHIFT; };
  void add(uint32_t v) {  _shiftedVal = ((K - 1) * _shiftedVal + (v << SHIFT)) / K; };
  uint32_t getValue() const { return _shiftedVal >> SHIFT; };

  void operator=(const uint32_t &v) { init(v); };
  operator uint32_t () const { return getValue(); };
private:
  uint32_t _shiftedVal;
};

static MovingAvg<DYNPOWER_LQ_MOVING_AVG_K, 16> dynpower_mavg_lq;
static MeanAccumulator<int32_t, int8_t, -128> dynpower_mean_rssi;
static StdevAccumulator dynpower_stat_snr;
static int8_t dynpower_updated;
static uint32_t dynpower_last_linkstats_millis;
static uint8_t dynpower_curr_rf_idx;

static void DynamicPower_SetToConfigPower()
{
    POWERMGNT::setPower((PowerLevels_e)config.GetPower());
}

void DynamicPower_Init()
{
    dynpower_mavg_lq = 100;
    dynpower_updated = DYNPOWER_UPDATE_NOUPDATE;
    dynpower_curr_rf_idx = 0;
}

void ICACHE_RAM_ATTR DynamicPower_TelemetryUpdate(int8_t snrScaled)
{
    dynpower_updated = snrScaled;
}

void DynamicPower_Update(uint32_t now)
{
  int8_t snrScaled = dynpower_updated;
  dynpower_updated = DYNPOWER_UPDATE_NOUPDATE;

  bool newTlmAvail = snrScaled > DYNPOWER_UPDATE_MISSED;
  bool lastTlmMissed = snrScaled == DYNPOWER_UPDATE_MISSED;

  int8_t rssi = (linkStats.active_antenna == 0) ? linkStats.uplink_RSSI_1 : linkStats.uplink_RSSI_2;
  if(ExpressLRS_currAirRate_RFperfParams->index != dynpower_curr_rf_idx)
  {
    dynpower_curr_rf_idx = ExpressLRS_currAirRate_RFperfParams->index;
    dynpower_stat_snr.reset();
  }

  // power is too strong and saturate the RX LNA
  if (newTlmAvail && (rssi >= -5))
  {
    DBGVLN("-power (overload)");
    POWERMGNT::decPower();
  }

  // When not using dynamic power, return here
  if (!config.GetDynamicPower())
  {
    // if RSSI is dropped enough, inc power back to the configured power
    if (newTlmAvail && (rssi <= -20) &&  POWERMGNT::currPower() < (PowerLevels_e)config.GetPower())
    {
      DynamicPower_SetToConfigPower();
    }
    return;
  }

  //
  // The rest of the codes should be executeded only if dynamic power config is enabled
  //

  const uint8_t rampup = std::min(config.GetDynamicPowerRampup(), (uint8_t)DYNPOWER_RAMPUP_LEVEL_MAX);

  // =============  DYNAMIC_POWER_BOOST: Switch-triggered power boost up ==============
  // Or if telemetry is lost while armed (done up here because dynpower_updated is only updated on telemetry)
  uint8_t boostChannel = config.GetBoostChannel();
  if ((connectionState == disconnected && isArmed) ||
    (boostChannel && (CRSF_to_BIT(ChannelData[AUX9 + boostChannel - 1]) == 0)))
  {
    DynamicPower_SetToConfigPower();
    return;
  }

  // How much available power is left for incremental increases
  uint8_t configPower = (uint8_t)config.GetPower();
  uint8_t currPower = (uint8_t)POWERMGNT::currPower();
  uint8_t powerHeadroom = (configPower > currPower) ? configPower - currPower : 0;

  if (lastTlmMissed)
  {
    // If armed and missing telemetry, raise the power, but only after the first LinkStats is missed (which come
    // at most every 512ms). This delays the first increase, then will bump it once for each missed TLM after that
    // state == connected is not used: unplugging an RX will be connected and will boost power to max before disconnect
    if (isArmed && (powerHeadroom > 0))
    {
      uint32_t linkstatsInterval = ExpressLRS_currTlmDenom * ExpressLRS_currAirRate_Modparams->interval / (1000U / 2U);
      linkstatsInterval = std::max(linkstatsInterval, (uint32_t)512U) * DYNPOWER_RAMPUP_TLM_NUM[rampup] / DYNPOWER_RAMPUP_TLM_DEN[rampup];
      if ((now - dynpower_last_linkstats_millis) > (linkstatsInterval + 2U))
      {
        DBGLN("+power (tlm)");
        POWERMGNT::incPower();
      }
    }
    return;
  }

  // If no new telemetry, no new LQ/SNR/RSSI to use for adjustment
  if (!newTlmAvail)
    return;
  dynpower_last_linkstats_millis = now;

  // =============  LQ-based power boost up ==============
  // Quick boost up of power when detected any emergency LQ drops.
  // It should be useful for bando or sudden lost of LoS cases.
  uint32_t lq_current = linkStats.uplink_Link_quality;
#if defined(Regulatory_Domain_EU_CE_2400)
  // Scale up receiver LQ for packets not sent because the channel was not clear
  // the calculation could exceed 100% during a rate change or initial connect when the LQs are not synced
  lq_current = std::min(lq_current * 100 / std::max((uint32_t)LBTSuccessCalc.getLQ(), (uint32_t)1U), (uint32_t)100U);
#endif
  uint32_t lq_avg = dynpower_mavg_lq;
  int32_t lq_diff = lq_avg - lq_current;
  dynpower_mavg_lq.add(lq_current);
  // if LQ drops quickly (DYNPOWER_LQ_BOOST_THRESH_DIFF) or critically low below DYNPOWER_LQ_BOOST_THRESH_MIN, immediately boost to the configured max power.
  if (lq_diff >= DYNPOWER_LQ_BOOST_THRESH_DIFF || lq_current <= DYNPOWER_LQ_BOOST_THRESH_MIN)
  {
      DynamicPower_SetToConfigPower();
      return;
  }

  int32_t expected_RXsensitivity = ExpressLRS_currAirRate_RFperfParams->RXsensitivity;
  PowerLevels_e startPowerLevel = POWERMGNT::currPower();

  if (ExpressLRS_currAirRate_RFperfParams->DynpowerSnrThreshUp == DYNPOWER_SNR_THRESH_NONE)
  {
    // =============  RSSI-based power increment ==============
    // a simple threshold compared against N sample average of
    // rssi vs the sensitivity limit +/- some thresholds
    dynpower_mean_rssi.add(rssi);

    if (dynpower_mean_rssi.getCount() >= DYNPOWER_RSSI_CNT)
    {
      int8_t rssi_dec_threshold = expected_RXsensitivity + DYNPOWER_RSSI_THRESH_DN + DYNPOWER_RAMPUP_RSSI_BONUS[rampup];
      // Cap the raise threshold at least 1dB below the lower threshold; a no-op given the shared bonus above, kept as defense-in-depth
      int8_t rssi_inc_threshold = static_cast<int8_t>(std::min(
          (int16_t)(expected_RXsensitivity + DYNPOWER_RSSI_THRESH_UP + DYNPOWER_RAMPUP_RSSI_BONUS[rampup]),
          (int16_t)(rssi_dec_threshold - 1)));
      int8_t avg_rssi = dynpower_mean_rssi.mean(); // resets it too
      if ((avg_rssi < rssi_inc_threshold) && (powerHeadroom > 0))
      {
        DBGLN("+power (rssi)");
        POWERMGNT::incPower();
      }
      else if (avg_rssi > rssi_dec_threshold && lq_avg >= DYNPOWER_RAMPUP_LQ_THRESH_DN[rampup])
      {
        DBGVLN("-power (rssi)"); // Verbose because this spams when idle
        POWERMGNT::decPower();
      }
    }
  } // ^^ if RSSI-based
  else
  {
    // default thresholds from the config (as a fallback)
    int8_t snr_stat_threshold_up = ExpressLRS_currAirRate_RFperfParams->DynpowerSnrThreshUp + DYNPOWER_RAMPUP_SNR_BONUS_SCALED[rampup];
    int8_t snr_stat_threshold_dn = ExpressLRS_currAirRate_RFperfParams->DynpowerSnrThreshDn + DYNPOWER_RAMPUP_SNR_BONUS_SCALED[rampup];

    // Incorporate the current value if LQ meets the desired LQ standard
    if (lq_current >= 99)
    {
        dynpower_stat_snr.add(snrScaled);
    }
    // is SNR stat ready? = is the buffer fully stuffed?
    if(dynpower_stat_snr.getCount() >= dynpower_stat_snr.WINDOW_SIZE)
    {
      static_assert(dynpower_stat_snr.FIXED_POINT_SHIFT >= 4, "StdDevAccumulator must be at least 4 bits of decimal fixed point");
      int32_t snr_stat_mean = dynpower_stat_snr.meanRaw() >> (dynpower_stat_snr.FIXED_POINT_SHIFT - 4);
      int32_t snr_stat_stdev = dynpower_stat_snr.standardDeviationRaw() >> (dynpower_stat_snr.FIXED_POINT_SHIFT - 4);

      // Fuzzy logic: reduce scale factor when LQ is getting low for more conservative power management
      // Base scale factor is DYNPOWER_RAMPUP_SNR_SIGMA_UP_NUM/4 sigma below the mean, reduce it proportionally when LQ < 100
      // (this LQ-based fuzz only applies to the raise side, same as before ramp-up presets existed)
      int32_t scale_factor_numerator = DYNPOWER_RAMPUP_SNR_SIGMA_UP_NUM[rampup];
      if (lq_current < 100) {
        // scale factor will be 1 (=-0.25 sd for power up threshold) when LQ is 85
        scale_factor_numerator = std::max((int32_t)(scale_factor_numerator - ((100 - lq_current) * (scale_factor_numerator - 1)) / 15), (int32_t)1);
      }

      int8_t snr_thre_up_scaled = static_cast<int8_t>((snr_stat_mean - (snr_stat_stdev * scale_factor_numerator) / 4) / 16);            // Dynamic scale based on LQ
      int8_t snr_thre_dn_scaled = static_cast<int8_t>((snr_stat_mean + (snr_stat_stdev * DYNPOWER_RAMPUP_SNR_SIGMA_DN_NUM[rampup]) / 4) / 16); // 0.5sd .. 2.0sd, growing with aggressiveness so lowering requires more margin above the mean
      int8_t snr_thre_up_limit = snr_thre_dn_scaled - SNR1dB;                                                                 // 1dB min threshold separation

      snr_stat_threshold_up = std::min(snr_thre_up_scaled, snr_thre_up_limit);
      snr_stat_threshold_dn = snr_thre_dn_scaled;
      //DBGLN("cur=%d tup=%d tdn=%d lim=%d mean=%d sd=%d", snrScaled, snr_thre_up_scaled, snr_thre_dn_scaled, snr_thre_up_limit, snr_stat_mean, snr_stat_stdev);
      //DBGLN("cur %d t_up %d t_dn %d lqavg %d", snrScaled, snr_stat_threshold_up, snr_stat_threshold_dn, lq_avg);
    }

    // =============  SNR-based power increment ==============
    // Decrease the power if SNR above threshold and LQ is good
    // Increase the power for each (X) SNR below the threshold
    if (snrScaled >= snr_stat_threshold_dn && lq_avg >= DYNPOWER_RAMPUP_LQ_THRESH_DN[rampup])
    {
      if(POWERMGNT::currPower() > MinPower) // prevent spamming when idle
      {
        DBGLN("-power (snr) %d >= %d", snrScaled, snr_stat_threshold_dn);
      }
      POWERMGNT::decPower();
    }

    // use RSSI_DN threshold to make sure the signal is still in good range but with some distance for SNR to exhibit a fair distribution
    // (ramped like the rest of this branch, so a stronger RSSI/LQ doesn't silently block the more aggressive presets'
    // SNR-based raise from ever firing -- e.g. strong RSSI + good LQ but degraded SNR from 2.4GHz interference)
    bool isSignalBad = (rssi <= (expected_RXsensitivity + DYNPOWER_RSSI_THRESH_DN + DYNPOWER_RAMPUP_RSSI_BONUS[rampup])) || (lq_avg <= DYNPOWER_RAMPUP_LQ_THRESH_DN[rampup]);

    while ((snrScaled <= snr_stat_threshold_up) && (powerHeadroom > 0) && isSignalBad)
    {
      DBGLN("+power (snr) %d <= %d", snrScaled, snr_stat_threshold_up);
      POWERMGNT::incPower();
      // Every power doubling will theoretically increase the SNR by 3dB, but closer to 2dB in testing
      snrScaled += SNR_SCALE(2);
      --powerHeadroom;
    }
  } // ^^ if SNR-based

  // If instant LQ is low, but the SNR/RSSI did nothing, inc power by one step
  // Cap at least 1% below the (also ramp-adjusted) lowering-permission threshold; a no-op given the values chosen above, kept as defense-in-depth
  uint8_t lqThreshUp = std::min(DYNPOWER_RAMPUP_LQ_THRESH_UP[rampup], (uint8_t)(DYNPOWER_RAMPUP_LQ_THRESH_DN[rampup] - 1));
  if ((powerHeadroom > 0) && (startPowerLevel == POWERMGNT::currPower()) && (lq_current <= lqThreshUp))
  {
    DBGLN("+power (lq)");
    POWERMGNT::incPower();
  }
}

#endif // TARGET_TX

#if defined(TARGET_RX)

/***
 * @brief: Set power to configured power or update power to match the current TX power
 * @param initialize: Set to true if calling from setup() to force even PWR_MATCH_TX to default power
*/
void DynamicPower_UpdateRx(bool initialize)
{
  if (config.GetPower() != PWR_MATCH_TX)
  {
    POWERMGNT::setPower((PowerLevels_e)config.GetPower());
  } /* !PWR_MATCH_TX (fixed power) */
  else
  {
    static uint8_t powerLevel = 0;
    if (linkStats.uplink_TX_Power != powerLevel)
    {
      powerLevel = linkStats.uplink_TX_Power;
      PowerLevels_e newPower = crsfPowerToPower(linkStats.uplink_TX_Power);
      DBGLN("Matching TX power %u", newPower);
      POWERMGNT::setPower(newPower);
    }
    else if (initialize)
    {
      POWERMGNT::setPower(POWERMGNT::getDefaultPower());
    }
  } /* PWR_MATCH_TX */
}

#endif // TARGET_RX
