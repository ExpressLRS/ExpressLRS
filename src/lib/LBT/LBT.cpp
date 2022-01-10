#include "LBT.h"
#include "SX1280Driver.h"

extern SX1280Driver Radio;

LQCALC<100> LBTSuccessCalc;

int8_t ICACHE_RAM_ATTR PowerEnumToLBTLimit(PowerLevels_e txPower)
{
    // Calculated from EN 300 328, adjusted for 800kHz BW for sx1280
    // TL = -70 dBm/MHz + 10*log10(0.8MHz) + 10 × log10 (100 mW / Pout) (Pout in mW e.i.r.p.)
    // TODO: This threshold should be modified with a config for each HW with antenna gain,
    // different RF frontends and also individual adjustment offset for differences in 
    // rssi between bandwidth setting.
    switch(txPower)
    {
      case PWR_10mW: return -61;
      case PWR_25mW: return -65;
      case PWR_50mW: return -68;
      case PWR_100mW: return -71;
      default: return -71; // Values above 100mW are not relevant, default to 100mW threshold
    }
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
