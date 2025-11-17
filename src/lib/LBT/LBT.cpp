#if defined(Regulatory_Domain_EU_CE_2400)
#include "common.h"
#include "logging.h"
#include "LBT.h"
#include "POWERMGNT.h"
#include "config.h"

LQCALC<100> LBTSuccessCalc;
static uint32_t rxStartTime;

#if !defined(LBT_RSSI_THRESHOLD_OFFSET_DB)
  #define LBT_RSSI_THRESHOLD_OFFSET_DB 0
#endif

bool LbtIsEnabled = false;
static uint32_t validRSSIdelayUs = 0;

static uint32_t SpreadingFactorToRSSIvalidDelayUs(uint8_t SF, uint8_t radio_type)
{
#if defined(RADIO_LR1121)
  if (radio_type == RADIO_TYPE_LR1121_LORA_2G4 || radio_type == RADIO_TYPE_LR1121_LORA_DUAL)
  {
    switch((lr11xx_radio_lora_sf_t)SF)
    {
      case LR11XX_RADIO_LORA_SF5: return 22;
      case LR11XX_RADIO_LORA_SF6: return 22;
      case LR11XX_RADIO_LORA_SF7: return 22;
      case LR11XX_RADIO_LORA_SF8: return 240;
      default: return 240;
    }
  }
  if (radio_type == RADIO_TYPE_LR1121_GFSK_2G4)
  {
    return 40; // 40us settling time; documentation says Twait for 467 kHz bandwidth is 30.68us
  }
#elif defined(RADIO_SX128X)
  // The necessary wait time from RX start to valid instant RSSI reading
  // changes with the spreading factor setting.
  // The worst case necessary wait time is TX->RX switch time + Lora symbol time
  // This assumes the radio switches from either TX, RX or FS (Freq Synth mode)
  // TX to RX switch time is 60us for sx1280
  // Lora symbol time for the relevant spreading factors is:
  // SF5: 39.4us
  // SF6: 78.8us
  // SF7: 157.6us
  // SF8: 315.2us
  // However, by measuring when the RSSI reading is stable and valid, it was found that
  // actual necessary wait times are:
  // SF5 ~100us (60us + SF5 symbol time)
  // SF6 ~141us (60us + SF6 symbol time)
  // SF7 ~218us (60us + SF7 symbol time)
  // SF8 ~376us (60us + SF8 symbol time) Empirical testing shows 480us to be the sweet-spot

  if (radio_type == RADIO_TYPE_SX128x_LORA)
  {
    switch((SX1280_RadioLoRaSpreadingFactors_t)SF)
    {
      case SX1280_LORA_SF5: return 100;
      case SX1280_LORA_SF6: return 141;
      case SX1280_LORA_SF7: return 218;
      case SX1280_LORA_SF8: return 480;
      default: return 480;
    }
  }
  if (radio_type == RADIO_TYPE_SX128x_FLRC)
  {
    return 60 + 20; // switching time (60us) + 20us settling time (seems fine when testing)
  }
#endif
  return 0;
}

void LbtEnableIfRequired()
{
    LbtIsEnabled = config.GetPower() > PWR_10mW;
#if defined(RADIO_LR1121)
    LbtIsEnabled = LbtIsEnabled && (ExpressLRS_currAirRate_Modparams->radio_type == RADIO_TYPE_LR1121_LORA_2G4 || ExpressLRS_currAirRate_Modparams->radio_type == RADIO_TYPE_LR1121_GFSK_2G4 || ExpressLRS_currAirRate_Modparams->radio_type == RADIO_TYPE_LR1121_LORA_DUAL);
#endif
    validRSSIdelayUs = SpreadingFactorToRSSIvalidDelayUs(ExpressLRS_currAirRate_Modparams->sf, ExpressLRS_currAirRate_Modparams->radio_type);
}

static int8_t ICACHE_RAM_ATTR PowerEnumToLBTLimit(PowerLevels_e txPower, uint8_t radio_type)
{
  // Calculated from EN 300 328, adjusted for 800kHz BW for sx1280
  // TL = -70 dBm/MHz + 10*log10(0.8MHz) + 10 × log10 (100 mW / Pout) (Pout in mW e.i.r.p.)
  // This threshold should be offset with a #define config for each HW that corrects for antenna gain,
  // different RF frontends.
  // TODO: Maybe individual adjustment offset for differences in
  // rssi reading between bandwidth setting is also necessary when other BW than 0.8MHz are used.
#if defined(RADIO_LR1121)
  if (radio_type == RADIO_TYPE_LR1121_LORA_2G4 || radio_type == RADIO_TYPE_LR1121_LORA_DUAL)
  {
    switch(txPower)
    {
      case PWR_10mW: return -61 + LBT_RSSI_THRESHOLD_OFFSET_DB;
      case PWR_25mW: return -65 + LBT_RSSI_THRESHOLD_OFFSET_DB;
      case PWR_50mW: return -68 + LBT_RSSI_THRESHOLD_OFFSET_DB;
      case PWR_100mW: return -71 + LBT_RSSI_THRESHOLD_OFFSET_DB;
      // Values above 100mW are not relevant, default to 100mW threshold
      default: return -71 + LBT_RSSI_THRESHOLD_OFFSET_DB;
    }
  }
  if (radio_type == RADIO_TYPE_LR1121_GFSK_2G4)
  {
    switch(txPower)
    {
      case PWR_10mW: return -63 + LBT_RSSI_THRESHOLD_OFFSET_DB;
      case PWR_25mW: return -67 + LBT_RSSI_THRESHOLD_OFFSET_DB;
      case PWR_50mW: return -70 + LBT_RSSI_THRESHOLD_OFFSET_DB;
      case PWR_100mW: return -73 + LBT_RSSI_THRESHOLD_OFFSET_DB;
      // Values above 100mW are not relevant, default to 100mW threshold
      default: return -73 + LBT_RSSI_THRESHOLD_OFFSET_DB;
    }
  }
#elif defined(RADIO_SX128X)
  if (radio_type == RADIO_TYPE_SX128x_LORA)
  {
    switch(txPower)
    {
      case PWR_10mW: return -61 + LBT_RSSI_THRESHOLD_OFFSET_DB;
      case PWR_25mW: return -65 + LBT_RSSI_THRESHOLD_OFFSET_DB;
      case PWR_50mW: return -68 + LBT_RSSI_THRESHOLD_OFFSET_DB;
      case PWR_100mW: return -71 + LBT_RSSI_THRESHOLD_OFFSET_DB;
      // Values above 100mW are not relevant, default to 100mW threshold
      default: return -71 + LBT_RSSI_THRESHOLD_OFFSET_DB;
    }
  }
  if (radio_type == RADIO_TYPE_SX128x_FLRC)
  {
    switch(txPower)
    {
      case PWR_10mW: return -63 + LBT_RSSI_THRESHOLD_OFFSET_DB;
      case PWR_25mW: return -67 + LBT_RSSI_THRESHOLD_OFFSET_DB;
      case PWR_50mW: return -70 + LBT_RSSI_THRESHOLD_OFFSET_DB;
      case PWR_100mW: return -73 + LBT_RSSI_THRESHOLD_OFFSET_DB;
      // Values above 100mW are not relevant, default to 100mW threshold
      default: return -73 + LBT_RSSI_THRESHOLD_OFFSET_DB;
    }
  }
#endif
  return 0;
}

void ICACHE_RAM_ATTR LbtCcaTimerStart(void)
{
  if (!LbtIsEnabled)
    return;

  rxStartTime = micros();
}

SX12XX_Radio_Number_t ICACHE_RAM_ATTR LbtChannelIsClear(SX12XX_Radio_Number_t radioNumber)
{
  if (radioNumber == SX12XX_Radio_NONE)
    return SX12XX_Radio_NONE;

  LBTSuccessCalc.inc(); // Increment count for every channel check

  if (!LbtIsEnabled)
  {
    LBTSuccessCalc.add();
    return radioNumber;
  }

  // Read rssi after waiting the minimum RSSI valid delay.
  // If this function is called long enough after RX enable,
  // this will always be ok on first try as is the case for TX.

  // TODO: Better way than busypolling this for RX?
  // this loop should only run for RX, where listen before talk RX is started right after FHSS hop
  // so there is no dead-time to run RX before telemetry TX is supposed to happen.
  // if flipping the logic, so telemetry TX happens right before FHSS hop, then TX-side ends up with polling here instead?
  // Maybe it could be skipped if there was only TX of telemetry happening when FHSShop does not happen.
  // Then RX for LBT could stay enabled from last received packet, and RSSI would be valid instantly.
  // But for now, FHSShops and telemetry rates does not divide evenly, so telemetry will some times happen
  // right after FHSS and we need wait here.

  const uint32_t elapsed = micros() - rxStartTime;
  if(elapsed < validRSSIdelayUs)
  {
    delayMicroseconds(validRSSIdelayUs - elapsed);
  }

  int8_t rssiInst1 = -128;
  int8_t rssiInst2 = -128;
  SX12XX_Radio_Number_t clearChannelsMask = SX12XX_Radio_NONE;
  const int8_t rssiCutOff = PowerEnumToLBTLimit(POWERMGNT::currPower(), ExpressLRS_currAirRate_Modparams->radio_type);

#if defined(RADIO_LR1121)
  Radio.StartRssiInst(radioNumber);
#endif
  if (radioNumber & SX12XX_Radio_1)
  {
    // If using dualband, radio1 is always SubGHz and no CCA is required.
    // Leave rssiInst1=-128 so it always passes.
    if (ExpressLRS_currAirRate_Modparams->radio_type != RADIO_TYPE_LR1121_LORA_DUAL)
    {
        rssiInst1 = Radio.GetRssiInst(SX12XX_Radio_1);
    }

    if(rssiInst1 < rssiCutOff)
    {
        clearChannelsMask |= SX12XX_Radio_1;
    }
  }

  if (isDualRadio() && radioNumber & SX12XX_Radio_2)
  {
    rssiInst2 = Radio.GetRssiInst(SX12XX_Radio_2);

    if(rssiInst2 < rssiCutOff)
    {
        clearChannelsMask |= SX12XX_Radio_2;
    }
  }

  // Useful to debug if and how long the rssi wait is, and rssi threshold rssiCutOff
  // if (clearChannelsMask != radioNumber)
  // {
  //   DBGLN("wait: %d, cutoff: %d, rssi: %d %d %d, %s", validRSSIdelayUs - elapsed, rssiCutOff, rssiInst1, rssiInst2, clearChannelsMask, clearChannelsMask ? "clear" : "in use");
  // }

  if(clearChannelsMask)
  {
    LBTSuccessCalc.add(); // Add success only when actually preparing for TX
  }

  return clearChannelsMask;
}
#endif
