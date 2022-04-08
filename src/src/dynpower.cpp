#if defined(TARGET_TX)

#include <dynpower.h>
#include <common.h>

#if !defined(DYNPOWER_THRESH_DN)
  #if defined(RADIO_SX127X)
    #define DYNPOWER_THRESH_DN              5
  #else
    #define DYNPOWER_THRESH_DN              10
  #endif
#endif
#define DYNAMIC_POWER_BOOST_LQ_THRESHOLD  20 // If LQ is dropped suddenly for this amount (relative), immediately boost to the max power configured.
#define DYNAMIC_POWER_BOOST_LQ_MIN        50 // If LQ is below this value (absolute), immediately boost to the max power configured.
#define DYNAMIC_POWER_MOVING_AVG_K         8 // Number of previous values for calculating moving average. Best with power of 2.

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

static MovingAvg<DYNAMIC_POWER_MOVING_AVG_K, 16> dynamic_power_avg_lq;
static DynamicPowerTelemetryUpdate_e dynamic_power_updated;

extern bool IsArmed();
extern volatile uint32_t LastTLMpacketRecvMillis;

static void DynamicPower_SetToConfigPower()
{
    POWERMGNT::setPower((PowerLevels_e)config.GetPower());
}

void DynamicPower_Init()
{
    dynamic_power_avg_lq = 100;
    dynamic_power_updated = dptuNoUpdate;
}

void ICACHE_RAM_ATTR DynamicPower_TelemetryUpdate(DynamicPowerTelemetryUpdate_e dptu)
{
    dynamic_power_updated = dptu;
}

void DynamicPower_Update(uint32_t now)
{
  bool newTlmAvail = dynamic_power_updated == dptuNewLinkstats;
  bool lastTlmMissed = dynamic_power_updated == dptuMissed;
  dynamic_power_updated = dptuNoUpdate;

  // Get the RSSI from the selected antenna.
  int8_t rssi = (CRSF::LinkStatistics.active_antenna == 0) ? CRSF::LinkStatistics.uplink_RSSI_1 : CRSF::LinkStatistics.uplink_RSSI_2;

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
    if (newTlmAvail && (rssi <= -20))
    {
      DynamicPower_SetToConfigPower();
    }
    return;
  }

  // The rest of the codes should be executeded only if dynamic power config is enabled

  // =============  DYNAMIC_POWER_BOOST: Switch-triggered power boost up ==============
  // Or if telemetry is lost while armed (done up here because dynamic_power_updated is only updated on telemetry)
  uint8_t boostChannel = config.GetBoostChannel();
  bool armed = IsArmed();
  if ((connectionState == disconnected && armed) ||
    (boostChannel && (CRSF_to_BIT(CRSF::ChannelDataIn[AUX9 + boostChannel - 1]) == 0)))
  {
    DynamicPower_SetToConfigPower();
    return;
  }

  // How much available power is left for incremental increases
  uint8_t powerHeadroom = config.GetPower() - (uint8_t)POWERMGNT::currPower();

  if (lastTlmMissed)
  {
    // If armed and missing telemetry, raise the power, but only after the first LinkStats is missed (which come
    // at most every 512ms). This delays the first increase, then will bump it once for each missed TLM after that
    // state == connected is not used: unplugging an RX will be connected and will boost power to max before disconnect
    if (armed && (powerHeadroom > 0))
    {
      uint32_t linkstatsInterval = 2U * TLMratioEnumToValue(config.GetTlm()) * ExpressLRS_currAirRate_Modparams->interval / 1000U;
      linkstatsInterval = std::max(linkstatsInterval, (uint32_t)512U);
      if (now - LastTLMpacketRecvMillis > (linkstatsInterval + 2U))
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

  // =============  LQ-based power boost up ==============
  // Quick boost up of power when detected any emergency LQ drops.
  // It should be useful for bando or sudden lost of LoS cases.
  uint32_t lq_current = CRSF::LinkStatistics.uplink_Link_quality;
  uint32_t lq_avg = dynamic_power_avg_lq;
  int32_t lq_diff = lq_avg - lq_current;
  // if LQ drops quickly (DYNAMIC_POWER_BOOST_LQ_THRESHOLD) or critically low below DYNAMIC_POWER_BOOST_LQ_MIN, immediately boost to the configured max power.
  if(lq_diff >= DYNAMIC_POWER_BOOST_LQ_THRESHOLD || lq_current <= DYNAMIC_POWER_BOOST_LQ_MIN)
  {
      DynamicPower_SetToConfigPower();
  }
  dynamic_power_avg_lq.add(lq_current);

  // =============  SNR-based power boost up ==============
  // Decrease the power if SNR above threshold and LQ is good
  // Increase the power for each (X) SNR below the threshold
  int8_t snr = CRSF::LinkStatistics.uplink_SNR;
  constexpr unsigned DYNPOWER_MIN_LQ_UP = 95;
  if (snr >= DYNPOWER_THRESH_DN && lq_avg >= DYNPOWER_MIN_LQ_UP)
  {
    DBGVLN("-power");
    POWERMGNT::decPower();
  }

  while ((snr <= ExpressLRS_currAirRate_RFperfParams->DynpowerUpThresholdSnr) && (powerHeadroom > 0))
  {
    DBGLN("+power");
    POWERMGNT::incPower();
    // Every power doubling will theoretically increase the SNR by 3dB, but closer to 2dB in testing
    snr += 2;
    --powerHeadroom;
  }
}

#endif // TARGET_TX