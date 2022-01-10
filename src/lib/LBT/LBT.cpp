#include "LBT.h"
#include "SX1280Driver.h"

extern SX1280Driver Radio;

LQCALC<100> LBTSuccessCalc;

#if !defined(LBT_RSSI_THRESHOLD_OFFSET_DB)
  #define LBT_RSSI_THRESHOLD_OFFSET_DB 0
#endif

int8_t ICACHE_RAM_ATTR PowerEnumToLBTLimit(PowerLevels_e txPower)
{
    // Calculated from EN 300 328, adjusted for 800kHz BW for sx1280
    // TL = -70 dBm/MHz + 10*log10(0.8MHz) + 10 × log10 (100 mW / Pout) (Pout in mW e.i.r.p.)
    // This threshold should be offset with a #define config for each HW that corrects for antenna gain,
    // different RF frontends.
    // TODO: Maybe individual adjustment offset for differences in
    // rssi reading between bandwidth setting is also necessary when other BW than 0.8MHz are used.
    int8_t lbtLimit = -71;
    switch(txPower)
    {
      case PWR_10mW: lbtLimit = -61;
      case PWR_25mW: lbtLimit = -65;
      case PWR_50mW: lbtLimit = -68;
      case PWR_100mW: lbtLimit = -71;
      default: lbtLimit = -71; // Values above 100mW are not relevant, default to 100mW threshold
    }
    return lbtLimit + LBT_RSSI_THRESHOLD_OFFSET_DB;
}

void ICACHE_RAM_ATTR BeginClearChannelAssessment(void)
{
  // Listen Before Talk (LBT) aka clear channel assessment (CCA)
  // Not interested in packets or interrupts while measuring RF energy on channel.
  Radio.SetDioIrqParams(SX1280_IRQ_RADIO_NONE, SX1280_IRQ_RADIO_NONE, SX1280_IRQ_RADIO_NONE, SX1280_IRQ_RADIO_NONE);
  Radio.RXnb();
}

bool ICACHE_RAM_ATTR ChannelIsClear(void)
{ 
  LBTSuccessCalc.inc(); // Increment count for every channel check

  // Read rssi until sensible reading. If this function 
  // is called long enough after RX enable, this will always be ok on first try, as is the case for TX.

  // TODO: Better way than busypolling this for RX?
  // this loop should only run for RX, where listen before talk RX is started right after FHSS hop
  // so there is no dead-time to run RX before telemetry TX is supposed to happen.
  // if flipping the logic, so telemetry TX happens right before FHSS hop, then TX-side ends up with polling here instead?
  
  uint8_t timeout = 0;
  int8_t instantRssi;  
  do
  {
    instantRssi = Radio.GetRssiInst();
  } while (instantRssi == -127 && timeout++ < 10);
  
  return instantRssi < PowerEnumToLBTLimit((PowerLevels_e)POWERMGNT::currPower());
}

void ICACHE_RAM_ATTR PrepareTXafterClearChannelAssessment(void)
{
  LBTSuccessCalc.add(); // Add success only when actually preparing for TX
  Radio.ClearIrqStatus(SX1280_IRQ_RADIO_ALL);
  Radio.SetDioIrqParams(SX1280_IRQ_TX_DONE, SX1280_IRQ_TX_DONE, SX1280_IRQ_RADIO_NONE, SX1280_IRQ_RADIO_NONE);
}

void ICACHE_RAM_ATTR PrepareRXafterClearChannelAssessment(void)
{
  // Go to idle and back to rx, to prevent packet reception during LBT filling the RX buffer
  Radio.SetTxIdleMode();
  Radio.ClearIrqStatus(SX1280_IRQ_RADIO_ALL);
  Radio.SetDioIrqParams(SX1280_IRQ_RX_DONE, SX1280_IRQ_RX_DONE, SX1280_IRQ_RADIO_NONE, SX1280_IRQ_RADIO_NONE);
}
