#include "rxtx_common.h"

#include "dynpower.h"
#include "lua.h"
#include "msp.h"
#include "telemetry_protocol.h"
#include "stubborn_receiver.h"
#include "stubborn_sender.h"

#include "devCRSF.h"
#include "devLED.h"
#include "devScreen.h"
#include "devBuzzer.h"
#include "devBLE.h"
#include "devLUA.h"
#include "devWIFI.h"
#include "devButton.h"
#include "devVTX.h"
#include "devGsensor.h"
#include "devThermal.h"
#include "devPDET.h"
#include "devBackpack.h"

//// CONSTANTS ////
#define MSP_PACKET_SEND_INTERVAL 10LU

/// define some libs to use ///
hwTimer hwTimer;
CRSF crsf;
POWERMGNT POWERMGNT;
MSP msp;
ELRS_EEPROM eeprom;
TxConfig config;
Stream *TxBackpack;
Stream *TxUSB;

// Variables / constants for Airport //
FIFO_GENERIC<AP_MAX_BUF_LEN> apInputBuffer;
FIFO_GENERIC<AP_MAX_BUF_LEN> apOutputBuffer;

#if defined(PLATFORM_ESP8266) || defined(PLATFORM_ESP32)
unsigned long rebootTime = 0;
extern bool webserverPreventAutoStart;
#endif
//// MSP Data Handling ///////
bool NextPacketIsMspData = false;  // if true the next packet will contain the msp data
char backpackVersion[32] = "";

////////////SYNC PACKET/////////
/// sync packet spamming on mode change vars ///
#define syncSpamAResidualTimeMS 500 // we spam some more after rate change to help link get up to speed
#define syncSpamAmount 3
volatile uint8_t syncSpamCounter = 0;
uint32_t rfModeLastChangedMS = 0;
uint32_t SyncPacketLastSent = 0;
////////////////////////////////////////////////

volatile uint32_t LastTLMpacketRecvMillis = 0;
uint32_t TLMpacketReported = 0;

LQCALC<25> LQCalc;

volatile bool busyTransmitting;
static volatile bool ModelUpdatePending;

bool InBindingMode = false;
uint8_t MSPDataPackage[5];
static uint8_t BindingSendCount;
bool RxWiFiReadyToSend = false;

static TxTlmRcvPhase_e TelemetryRcvPhase = ttrpTransmitting;
StubbornReceiver TelemetryReceiver;
StubbornSender MspSender;
uint8_t CRSFinBuffer[CRSF_MAX_PACKET_LEN+1];

device_affinity_t ui_devices[] = {
  {&CRSF_device, 1},
#ifdef HAS_LED
  {&LED_device, 0},
#endif
#ifdef HAS_RGB
  {&RGB_device, 0},
#endif
  {&LUA_device, 1},
#if defined(USE_TX_BACKPACK)
  {&Backpack_device, 0},
#endif
#ifdef HAS_BLE
  {&BLE_device, 0},
#endif
#ifdef HAS_BUZZER
  {&Buzzer_device, 0},
#endif
#ifdef HAS_WIFI
  {&WIFI_device, 0},
#endif
#ifdef HAS_BUTTON
  {&Button_device, 0},
#endif
#ifdef HAS_SCREEN
  {&Screen_device, 0},
#endif
#ifdef HAS_GSENSOR
  {&Gsensor_device, 0},
#endif
#if defined(HAS_THERMAL) || defined(HAS_FAN)
  {&Thermal_device, 0},
#endif
#if defined(GPIO_PIN_PA_PDET)
  {&PDET_device, 0},
#endif
  {&VTX_device, 0}
};

#if defined(GPIO_PIN_ANT_CTRL)
    static bool diversityAntennaState = LOW;
#endif

#ifdef TARGET_TX_GHOST
extern "C"
/**
  * @brief This function handles external line 2 interrupt request.
  * @param  None
  * @retval None
  */
void EXTI2_TSC_IRQHandler()
{
  HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_2);
}
#endif

void switchDiversityAntennas()
{
  if (GPIO_PIN_ANT_CTRL != UNDEF_PIN)
  {
    diversityAntennaState = !diversityAntennaState;
    digitalWrite(GPIO_PIN_ANT_CTRL, diversityAntennaState);
  }
  if (GPIO_PIN_ANT_CTRL_COMPL != UNDEF_PIN)
  {
    digitalWrite(GPIO_PIN_ANT_CTRL_COMPL, !diversityAntennaState);
  }
}

void ICACHE_RAM_ATTR LinkStatsFromOta(OTA_LinkStats_s * const ls)
{
  int8_t snrScaled = ls->SNR;
  DynamicPower_TelemetryUpdate(snrScaled);

  // Antenna is the high bit in the RSSI_1 value
  // RSSI received is signed, inverted polarity (positive value = -dBm)
  // OpenTX's value is signed and will display +dBm and -dBm properly
  crsf.LinkStatistics.uplink_RSSI_1 = -(ls->uplink_RSSI_1);
  crsf.LinkStatistics.uplink_RSSI_2 = -(ls->uplink_RSSI_2);
  crsf.LinkStatistics.uplink_Link_quality = ls->lq;
#if defined(DEBUG_FREQ_CORRECTION)
  // Don't descale the FreqCorrection value being send in SNR
  crsf.LinkStatistics.uplink_SNR = snrScaled;
#else
  crsf.LinkStatistics.uplink_SNR = SNR_DESCALE(snrScaled);
#endif
  crsf.LinkStatistics.active_antenna = ls->antenna;
  connectionHasModelMatch = ls->modelMatch;
  // -- downlink_SNR / downlink_RSSI is updated for any packet received, not just Linkstats
  // -- uplink_TX_Power is updated when sending to the handset, so it updates when missing telemetry
  // -- rf_mode is updated when we change rates
  // -- downlink_Link_quality is updated before the LQ period is incremented
  MspSender.ConfirmCurrentPayload(ls->mspConfirm);
}

bool ICACHE_RAM_ATTR ProcessTLMpacket(SX12xxDriverCommon::rx_status const status)
{
  if (status != SX12xxDriverCommon::SX12XX_RX_OK)
  {
    DBGLN("TLM HW CRC error");
    return false;
  }

  OTA_Packet_s * const otaPktPtr = (OTA_Packet_s * const)Radio.RXdataBuffer;
  if (!OtaValidatePacketCrc(otaPktPtr))
  {
    DBGLN("TLM crc error");
    return false;
  }

  if (otaPktPtr->std.type != PACKET_TYPE_TLM)
  {
    DBGLN("TLM type error %d", otaPktPtr->std.type);
    return false;
  }

  LastTLMpacketRecvMillis = millis();
  LQCalc.add();

  Radio.GetLastPacketStats();
  crsf.LinkStatistics.downlink_SNR = SNR_DESCALE(Radio.LastPacketSNRRaw);
  crsf.LinkStatistics.downlink_RSSI = Radio.LastPacketRSSI;

  // Full res mode
  if (OtaIsFullRes)
  {
    OTA_Packet8_s * const ota8 = (OTA_Packet8_s * const)otaPktPtr;
    uint8_t *telemPtr;
    uint8_t dataLen;
    if (ota8->tlm_dl.containsLinkStats)
    {
      LinkStatsFromOta(&ota8->tlm_dl.ul_link_stats.stats);
      telemPtr = ota8->tlm_dl.ul_link_stats.payload;
      dataLen = sizeof(ota8->tlm_dl.ul_link_stats.payload);
    }
    else
    {
      if (firmwareOptions.is_airport)
      {
        OtaUnpackAirportData(otaPktPtr, &apOutputBuffer);
        return true;
      }
      telemPtr = ota8->tlm_dl.payload;
      dataLen = sizeof(ota8->tlm_dl.payload);
    }
    //DBGLN("pi=%u len=%u", ota8->tlm_dl.packageIndex, dataLen);
    TelemetryReceiver.ReceiveData(ota8->tlm_dl.packageIndex, telemPtr, dataLen);
  }
  // Std res mode
  else
  {
    switch (otaPktPtr->std.tlm_dl.type)
    {
      case ELRS_TELEMETRY_TYPE_LINK:
        LinkStatsFromOta(&otaPktPtr->std.tlm_dl.ul_link_stats.stats);
        break;

      case ELRS_TELEMETRY_TYPE_DATA:
        if (firmwareOptions.is_airport)
        {
          OtaUnpackAirportData(otaPktPtr, &apOutputBuffer);
          return true;
        }
        TelemetryReceiver.ReceiveData(otaPktPtr->std.tlm_dl.packageIndex,
          otaPktPtr->std.tlm_dl.payload,
          sizeof(otaPktPtr->std.tlm_dl.payload));
        break;
    }
  }
  return true;
}

expresslrs_tlm_ratio_e ICACHE_RAM_ATTR UpdateTlmRatioEffective()
{
  expresslrs_tlm_ratio_e ratioConfigured = (expresslrs_tlm_ratio_e)config.GetTlm();
  // default is suggested rate for TLM_RATIO_STD/TLM_RATIO_DISARMED
  expresslrs_tlm_ratio_e retVal = ExpressLRS_currAirRate_Modparams->TLMinterval;
  bool updateTelemDenom = true;

  // TLM ratio is boosted for one sync cycle when the MspSender goes active
  if (MspSender.IsActive())
  {
    retVal = TLM_RATIO_1_2;
  }
  // If Armed, telemetry is disabled, otherwise use STD
  else if (ratioConfigured == TLM_RATIO_DISARMED)
  {
    if (crsf.IsArmed())
    {
      retVal = TLM_RATIO_NO_TLM;
      // Avoid updating ExpressLRS_currTlmDenom until connectionState == disconnected
      if (connectionState == connected)
        updateTelemDenom = false;
    }
  }
  else if (ratioConfigured != TLM_RATIO_STD)
  {
    retVal = ratioConfigured;
  }

  if (updateTelemDenom)
  {
    uint8_t newTlmDenom = TLMratioEnumToValue(retVal);
    // Delay going into disconnected state when the TLM ratio increases
    if (connectionState == connected && ExpressLRS_currTlmDenom > newTlmDenom)
      LastTLMpacketRecvMillis = SyncPacketLastSent;
    ExpressLRS_currTlmDenom = newTlmDenom;
  }

  return retVal;
}

void ICACHE_RAM_ATTR GenerateSyncPacketData(OTA_Sync_s * const syncPtr)
{
  const uint8_t SwitchEncMode = config.GetSwitchMode();
  const uint8_t Index = (syncSpamCounter) ? config.GetRate() : ExpressLRS_currAirRate_Modparams->index;

  if (syncSpamCounter)
    --syncSpamCounter;
  SyncPacketLastSent = millis();

  expresslrs_tlm_ratio_e newTlmRatio = UpdateTlmRatioEffective();

  syncPtr->fhssIndex = FHSSgetCurrIndex();
  syncPtr->nonce = OtaNonce;
  syncPtr->rateIndex = Index;
  syncPtr->newTlmRatio = newTlmRatio - TLM_RATIO_NO_TLM;
  syncPtr->switchEncMode = SwitchEncMode;
  syncPtr->UID3 = UID[3];
  syncPtr->UID4 = UID[4];
  syncPtr->UID5 = UID[5];

  // For model match, the last byte of the binding ID is XORed with the inverse of the modelId
  if (!InBindingMode && config.GetModelMatch())
  {
    syncPtr->UID5 ^= (~crsf.getModelID()) & MODELMATCH_MASK;
  }
}

uint8_t adjustPacketRateForBaud(uint8_t rateIndex)
{
  #if defined(RADIO_SX128X)
    if (crsf.GetCurrentBaudRate() == 115200) // Packet rate limited to 250Hz if we are on 115k baud
    {
      rateIndex = get_elrs_HandsetRate_max(rateIndex, 4000);
    }
    else if (crsf.GetCurrentBaudRate() == 400000) // Packet rate limited to 500Hz if we are on 400k baud
    {
      rateIndex = get_elrs_HandsetRate_max(rateIndex, 2000);
    }
  #endif
  return rateIndex;
}

void ICACHE_RAM_ATTR SetRFLinkRate(uint8_t index) // Set speed of RF link (hz)
{
  index = adjustPacketRateForBaud(index);
  expresslrs_mod_settings_s *const ModParams = get_elrs_airRateConfig(index);
  expresslrs_rf_pref_params_s *const RFperf = get_elrs_RFperfParams(index);
  bool invertIQ = UID[5] & 0x01;
  OtaSwitchMode_e newSwitchMode = (OtaSwitchMode_e)config.GetSwitchMode();

  if ((ModParams == ExpressLRS_currAirRate_Modparams)
    && (RFperf == ExpressLRS_currAirRate_RFperfParams)
    && (invertIQ == Radio.IQinverted)
    && (OtaSwitchModeCurrent == newSwitchMode))
    return;

  DBGLN("set rate %u", index);
  uint32_t interval = ModParams->interval;
#if defined(DEBUG_FREQ_CORRECTION) && defined(RADIO_SX128X)
  interval = interval * 12 / 10; // increase the packet interval by 20% to allow adding packet header
#endif
  hwTimer.updateInterval(interval);
  Radio.Config(ModParams->bw, ModParams->sf, ModParams->cr, GetInitialFreq(),
               ModParams->PreambleLen, invertIQ, ModParams->PayloadLength, ModParams->interval
#if defined(RADIO_SX128X)
               , uidMacSeedGet(), OtaCrcInitializer, (ModParams->radio_type == RADIO_TYPE_SX128x_FLRC)
#endif
               );

  if (isDualRadio() && config.GetAntennaMode() == TX_RADIO_MODE_GEMINI) // Gemini mode
  {
    Radio.SetFrequencyReg(FHSSgetInitialGeminiFreq(), SX12XX_Radio_2);
  }

  OtaUpdateSerializers(newSwitchMode, ModParams->PayloadLength);
  MspSender.setMaxPackageIndex(ELRS_MSP_MAX_PACKAGES);
  TelemetryReceiver.setMaxPackageIndex(OtaIsFullRes ? ELRS8_TELEMETRY_MAX_PACKAGES : ELRS4_TELEMETRY_MAX_PACKAGES);

  ExpressLRS_currAirRate_Modparams = ModParams;
  ExpressLRS_currAirRate_RFperfParams = RFperf;
  crsf.LinkStatistics.rf_Mode = ModParams->enum_rate;

  crsf.setSyncParams(interval * ExpressLRS_currAirRate_Modparams->numOfSends);
  connectionState = disconnected;
  rfModeLastChangedMS = millis();
}

void ICACHE_RAM_ATTR HandleFHSS()
{
  uint8_t modresult = (OtaNonce + 1) % ExpressLRS_currAirRate_Modparams->FHSShopInterval;
  // If the next packet should be on the next FHSS frequency, do the hop
  if (!InBindingMode && modresult == 0)
  {
    if (isDualRadio() && config.GetAntennaMode() == TX_RADIO_MODE_GEMINI) // Gemini mode
    {
      Radio.SetFrequencyReg(FHSSgetNextFreq(), SX12XX_Radio_1);
      Radio.SetFrequencyReg(FHSSgetGeminiFreq(), SX12XX_Radio_2);
    }
    else
    {
      Radio.SetFrequencyReg(FHSSgetNextFreq());
    }
  }
}

void ICACHE_RAM_ATTR HandlePrepareForTLM()
{
  // If TLM enabled and next packet is going to be telemetry, start listening to have a large receive window (time-wise)
  if (ExpressLRS_currTlmDenom != 1 && ((OtaNonce + 1) % ExpressLRS_currTlmDenom) == 0)
  {
    Radio.RXnb();
    TelemetryRcvPhase = ttrpPreReceiveGap;
  }
}

void ICACHE_RAM_ATTR SendRCdataToRF()
{
  uint32_t const now = millis();
  // ESP requires word aligned buffer
  WORD_ALIGNED_ATTR OTA_Packet_s otaPkt = {0};
  static uint8_t syncSlot;

  const bool isTlmDisarmed = config.GetTlm() == TLM_RATIO_DISARMED;
  uint32_t SyncInterval = (connectionState == connected && !isTlmDisarmed) ? ExpressLRS_currAirRate_RFperfParams->SyncPktIntervalConnected : ExpressLRS_currAirRate_RFperfParams->SyncPktIntervalDisconnected;
  bool skipSync = InBindingMode ||
    // TLM_RATIO_DISARMED keeps sending sync packets even when armed until the RX stops sending telemetry and the TLM=Off has taken effect
    (isTlmDisarmed && crsf.IsArmed() && (ExpressLRS_currTlmDenom == 1));

  uint8_t NonceFHSSresult = OtaNonce % ExpressLRS_currAirRate_Modparams->FHSShopInterval;
  bool WithinSyncSpamResidualWindow = now - rfModeLastChangedMS < syncSpamAResidualTimeMS;

  // Sync spam only happens on slot 1 and 2 and can't be disabled
  if ((syncSpamCounter || WithinSyncSpamResidualWindow) && (NonceFHSSresult == 1 || NonceFHSSresult == 2))
  {
    otaPkt.std.type = PACKET_TYPE_SYNC;
    GenerateSyncPacketData(OtaIsFullRes ? &otaPkt.full.sync.sync : &otaPkt.std.sync);
    syncSlot = 0; // reset the sync slot in case the new rate (after the syncspam) has a lower FHSShopInterval
  }
  // Regular sync rotates through 4x slots, twice on each slot, and telemetry pushes it to the next slot up
  // But only on the sync FHSS channel and with a timed delay between them
  else if ((!skipSync) && ((syncSlot / 2) <= NonceFHSSresult) && (now - SyncPacketLastSent > SyncInterval) && (Radio.currFreq == GetInitialFreq()))
  {
    otaPkt.std.type = PACKET_TYPE_SYNC;
    GenerateSyncPacketData(OtaIsFullRes ? &otaPkt.full.sync.sync : &otaPkt.std.sync);
    syncSlot = (syncSlot + 1) % (ExpressLRS_currAirRate_Modparams->FHSShopInterval * 2);
  }
  else
  {
    if (NextPacketIsMspData && MspSender.IsActive())
    {
      otaPkt.std.type = PACKET_TYPE_MSPDATA;
      if (OtaIsFullRes)
      {
        otaPkt.full.msp_ul.packageIndex = MspSender.GetCurrentPayload(
          otaPkt.full.msp_ul.payload,
          sizeof(otaPkt.full.msp_ul.payload));
      }
      else
      {
        otaPkt.std.msp_ul.packageIndex = MspSender.GetCurrentPayload(
          otaPkt.std.msp_ul.payload,
          sizeof(otaPkt.std.msp_ul.payload));
      }

      // send channel data next so the channel messages also get sent during msp transmissions
      NextPacketIsMspData = false;
      // counter can be increased even for normal msp messages since it's reset if a real bind message should be sent
      BindingSendCount++;
      // If the telemetry ratio isn't already 1:2, send a sync packet to boost it
      // to add bandwidth for the reply
      if (ExpressLRS_currTlmDenom != 2)
        syncSpamCounter = 1;
    }
    else
    {
      // always enable msp after a channel package since the slot is only used if MspSender has data to send
      NextPacketIsMspData = true;

      if (firmwareOptions.is_airport)
      {
        OtaPackAirportData(&otaPkt, &apInputBuffer);
      }
      else
      {
        OtaPackChannelData(&otaPkt, ChannelData, TelemetryReceiver.GetCurrentConfirm(), ExpressLRS_currTlmDenom);
      }
    }
  }

  ///// Next, Calculate the CRC and put it into the buffer /////
  OtaGeneratePacketCrc(&otaPkt);

  SX12XX_Radio_Number_t transmittingRadio = SX12XX_Radio_Default;

  if (isDualRadio())
  {
    switch (config.GetAntennaMode())
    {
    case TX_RADIO_MODE_GEMINI:
      transmittingRadio = SX12XX_Radio_All; // Gemini mode
      break;
    case TX_RADIO_MODE_ANT_1:
      transmittingRadio = SX12XX_Radio_1; // Single antenna tx and true diversity rx for tlm receiption.
      break;
    case TX_RADIO_MODE_ANT_2:
      transmittingRadio = SX12XX_Radio_2; // Single antenna tx and true diversity rx for tlm receiption.
      break;
    case TX_RADIO_MODE_SWITCH:
      if(OtaNonce%2==0)   transmittingRadio = SX12XX_Radio_1; // Single antenna tx and true diversity rx for tlm receiption.
      else   transmittingRadio = SX12XX_Radio_2; // Single antenna tx and true diversity rx for tlm receiption.
      break;
    default:
      break;
    }
  }

  SX12XX_Radio_Number_t clearChannelsMask = SX12XX_Radio_All;
#if defined(Regulatory_Domain_EU_CE_2400)
  clearChannelsMask = ChannelIsClear(transmittingRadio);
  if (clearChannelsMask)
#endif
  {
    Radio.TXnb((uint8_t*)&otaPkt, ExpressLRS_currAirRate_Modparams->PayloadLength, transmittingRadio & clearChannelsMask);
  }
}

/*
 * Called as the TOCK timer ISR when there is a CRSF connection from the handset
 */
void ICACHE_RAM_ATTR timerCallbackNormal()
{
#if defined(Regulatory_Domain_EU_CE_2400)
  if(!LBTSuccessCalc.currentIsSet())
  {
    Radio.TXdoneCallback();
  }
#endif

  // Sync OpenTX to this point
  if (!(OtaNonce % ExpressLRS_currAirRate_Modparams->numOfSends))
  {
    crsf.JustSentRFpacket();
  }

  // Do not transmit or advance FHSS/Nonce until in disconnected/connected state
  if (connectionState == awaitingModelId)
    return;

  // Tx Antenna Diversity
  if ((OtaNonce % ExpressLRS_currAirRate_Modparams->numOfSends == 0 || // Swicth with new packet data
      OtaNonce % ExpressLRS_currAirRate_Modparams->numOfSends == ExpressLRS_currAirRate_Modparams->numOfSends / 2) && // Swicth in the middle of DVDA sends
      TelemetryRcvPhase == ttrpTransmitting) // Only switch when transmitting.  A diversity rx will send tlm back on the best antenna.  So dont switch away from it.
  {
    switchDiversityAntennas();
  }

  // Nonce advances on every timer tick
  if (!InBindingMode)
    OtaNonce++;

  // If HandleTLM has started Receive mode, TLM packet reception should begin shortly
  // Skip transmitting on this slot
  if (TelemetryRcvPhase == ttrpPreReceiveGap)
  {
    TelemetryRcvPhase = ttrpExpectingTelem;
#if defined(Regulatory_Domain_EU_CE_2400)
    // Use downlink LQ for LBT success ratio instead for EU/CE reg domain
    crsf.LinkStatistics.downlink_Link_quality = LBTSuccessCalc.getLQ();
#else
    crsf.LinkStatistics.downlink_Link_quality = LQCalc.getLQ();
#endif
    LQCalc.inc();
    return;
  }
  else if (TelemetryRcvPhase == ttrpExpectingTelem && !LQCalc.currentIsSet())
  {
    // Indicate no telemetry packet received to the DP system
    DynamicPower_TelemetryUpdate(DYNPOWER_UPDATE_MISSED);
  }
  TelemetryRcvPhase = ttrpTransmitting;

#if defined(Regulatory_Domain_EU_CE_2400)
    BeginClearChannelAssessment(); // Get RSSI reading here, used also for next TX if in receiveMode.
#endif

  // Do not send a stale channels packet to the RX if one has not been received from the handset
  // *Do* send data if a packet has never been received from handset and the timer is running
  //     this is the case when bench testing and TXing without a handset
  uint32_t lastRcData = crsf.GetRCdataLastRecv();
  if (!lastRcData || (micros() - lastRcData < 1000000))
  {
    busyTransmitting = true;
    SendRCdataToRF();
  }
}

/*
 * Called as the timer ISR while waiting for eeprom flush
 */
void ICACHE_RAM_ATTR timerCallbackIdle()
{
  OtaNonce++;
  if ((OtaNonce + 1) % ExpressLRS_currAirRate_Modparams->FHSShopInterval == 0)
    ++FHSSptr;
}

static void UARTdisconnected()
{
  hwTimer.stop();
  connectionState = noCrossfire;
}

static void UARTconnected()
{
  #if defined(PLATFORM_ESP32) || defined(PLATFORM_ESP8266)
  webserverPreventAutoStart = true;
  #endif
  rfModeLastChangedMS = millis(); // force syncspam on first packets
  SetRFLinkRate(config.GetRate());
  if (connectionState == noCrossfire || connectionState < MODE_STATES)
  {
    // When CRSF first connects, always go into a brief delay before
    // starting to transmit, to make sure a ModelID update isn't coming
    // right behind it
    connectionState = awaitingModelId;
  }
  // But start the timer to get OpenTX sync going and a ModelID update sent
  hwTimer.resume();
}

void ResetPower()
{
  // Dynamic Power starts at MinPower unless armed
  // (user may be turning up the power while flying and dropping the power may compromise the link)
  if (config.GetDynamicPower())
  {
    if (!crsf.IsArmed())
    {
      // if dynamic power enabled and not armed then set to MinPower
      POWERMGNT.setPower(MinPower);
    }
    else if (POWERMGNT.currPower() < config.GetPower())
    {
      // if the new config is a higher power then set it, otherwise leave it alone
      POWERMGNT.setPower((PowerLevels_e)config.GetPower());
    }
  }
  else
  {
    POWERMGNT.setPower((PowerLevels_e)config.GetPower());
  }
  // TLM interval is set on the next SYNC packet
#if defined(Regulatory_Domain_EU_CE_2400)
  LBTEnabled = (config.GetPower() > PWR_10mW);
#endif
}

static void ChangeRadioParams()
{
  ModelUpdatePending = false;
  SetRFLinkRate(config.GetRate());
  ResetPower();
}

void ModelUpdateReq()
{
  // Force synspam with the current rate parameters in case already have a connection established
  if (config.SetModelId(crsf.getModelID()))
  {
    syncSpamCounter = syncSpamAmount;
    ModelUpdatePending = true;
  }

  // Jump from awaitingModelId to transmitting to break the startup delay now
  // that the ModelID has been confirmed by the handset
  if (connectionState == awaitingModelId)
  {
    connectionState = disconnected;
  }
}

static void ConfigChangeCommit()
{
  // Write the uncommitted eeprom values (may block for a while)
  config.Commit();
  // Change params after the blocking finishes as a rate change will change the radio freq
  ChangeRadioParams();
  // Resume the timer, will take one hop for the radio to be on the right frequency if we missed a hop
  hwTimer.callbackTock = &timerCallbackNormal;
  // UpdateFolderNames is expensive so it is called directly instead of in event() which gets called a lot
  luadevUpdateFolderNames();
  devicesTriggerEvent();
}

static void CheckConfigChangePending()
{
  if (config.IsModified() || ModelUpdatePending)
  {
    // Keep transmitting sync packets until the spam counter runs out
    if (syncSpamCounter > 0)
      return;

#if !defined(PLATFORM_STM32) || defined(TARGET_USE_EEPROM)
    while (busyTransmitting); // wait until no longer transmitting
#else
    // The code expects to enter here shortly after the tock ISR has started sending the last
    // sync packet, before the tick ISR. Because the EEPROM write takes so long and disables
    // interrupts, FastForward the timer
    const uint32_t EEPROM_WRITE_DURATION = 30000; // us, a page write on F103C8 takes ~29.3ms
    const uint32_t cycleInterval = ExpressLRS_currAirRate_Modparams->interval;
    // Total time needs to be at least DURATION, rounded up to next cycle
    // adding one cycle that will be eaten by busywaiting for the transmit to end
    uint32_t pauseCycles = ((EEPROM_WRITE_DURATION + cycleInterval - 1) / cycleInterval) + 1;
    // Pause won't return until paused, and has just passed the tick ISR (but not fired)
    hwTimer.pause(pauseCycles * cycleInterval);

    while (busyTransmitting); // wait until no longer transmitting

    --pauseCycles; // the last cycle will actually be a transmit
    while (pauseCycles--)
      timerCallbackIdle();
#endif
    // Prevent any other RF SPI traffic during the commit from RX or scheduled TX
    hwTimer.callbackTock = &timerCallbackIdle;
    // If telemetry expected in the next interval, the radio was in RX mode
    // and will skip sending the next packet when the timer resumes.
    // Return to normal send mode because if the skipped packet happened
    // to be on the last slot of the FHSS the skip will prevent FHSS
    if (TelemetryRcvPhase != ttrpTransmitting)
    {
      Radio.SetTxIdleMode();
      TelemetryRcvPhase = ttrpTransmitting;
    }
    ConfigChangeCommit();
  }
}

bool ICACHE_RAM_ATTR RXdoneISR(SX12xxDriverCommon::rx_status const status)
{
  if (LQCalc.currentIsSet())
  {
    return false; // Already received tlm, do not run ProcessTLMpacket() again.
  }

  bool packetSuccessful = ProcessTLMpacket(status);
  busyTransmitting = false;
  return packetSuccessful;
}

void ICACHE_RAM_ATTR TXdoneISR()
{
  if (!busyTransmitting)
  {
    return; // Already finished transmission and do not call HandleFHSS() a second time, which may hop the frequency!
  }

  if (connectionState != awaitingModelId)
  {
    HandleFHSS();
    HandlePrepareForTLM();
#if defined(Regulatory_Domain_EU_CE_2400)
    if (TelemetryRcvPhase != ttrpPreReceiveGap)
    {
      // Start RX for Listen Before Talk early because it takes about 100us
      // from RX enable to valid instant RSSI values are returned.
      // If rx was already started by TLM prepare above, this call will let RX
      // continue as normal.
      BeginClearChannelAssessment();
    }
#endif // non-CE
  }
  busyTransmitting = false;
}

static void UpdateConnectDisconnectStatus()
{
  // Number of telemetry packets which can be lost in a row before going to disconnected state
  constexpr unsigned RX_LOSS_CNT = 5;
  // Must be at least 512ms and +2 to account for any rounding down and partial millis()
  const uint32_t msConnectionLostTimeout = std::max((uint32_t)512U,
    (uint32_t)ExpressLRS_currTlmDenom * ExpressLRS_currAirRate_Modparams->interval / (1000U / RX_LOSS_CNT)
    ) + 2U;
  // Capture the last before now so it will always be <= now
  const uint32_t lastTlmMillis = LastTLMpacketRecvMillis;
  const uint32_t now = millis();
  if (lastTlmMillis && ((now - lastTlmMillis) <= msConnectionLostTimeout))
  {
    if (connectionState != connected)
    {
      connectionState = connected;
      crsf.ForwardDevicePings = true;
      DBGLN("got downlink conn");

      if (firmwareOptions.is_airport)
      {
        apInputBuffer.flush();
        apOutputBuffer.flush();
      }
    }
  }
  // If past RX_LOSS_CNT, or in awaitingModelId state for longer than DisconnectTimeoutMs, go to disconnected
  else if (connectionState == connected ||
    (now - rfModeLastChangedMS) > ExpressLRS_currAirRate_RFperfParams->DisconnectTimeoutMs)
  {
    connectionState = disconnected;
    connectionHasModelMatch = true;
    crsf.ForwardDevicePings = false;
  }
}

void SetSyncSpam()
{
  // Send sync spam if a UI device has requested to and the config has changed
  if (config.IsModified())
  {
    syncSpamCounter = syncSpamAmount;
  }
}

static void SendRxWiFiOverMSP()
{
  MSPDataPackage[0] = MSP_ELRS_SET_RX_WIFI_MODE;
  MspSender.SetDataToTransmit(MSPDataPackage, 1);
}

void SendRxLoanOverMSP()
{
  MSPDataPackage[0] = MSP_ELRS_SET_RX_LOAN_MODE;
  MspSender.SetDataToTransmit(MSPDataPackage, 1);
}

static void CheckReadyToSend()
{
  if (RxWiFiReadyToSend)
  {
    RxWiFiReadyToSend = false;
    if (!crsf.IsArmed())
    {
      SendRxWiFiOverMSP();
    }
  }
}

#if !defined(CRITICAL_FLASH)
void OnPowerGetCalibration(mspPacket_t *packet)
{
  uint8_t index = packet->readByte();
  UNUSED(index);
  int8_t values[PWR_COUNT] = {0};
  POWERMGNT.GetPowerCaliValues(values, PWR_COUNT);
  DBGLN("power get calibration value %d",  values[index]);
}

void OnPowerSetCalibration(mspPacket_t *packet)
{
  uint8_t index = packet->readByte();
  int8_t value = packet->readByte();

  if((index < 0) || (index > PWR_COUNT))
  {
    DBGLN("calibration error index %d out of range", index);
    return;
  }
  hwTimer.stop();
  delay(20);

  int8_t values[PWR_COUNT] = {0};
  POWERMGNT.GetPowerCaliValues(values, PWR_COUNT);
  values[index] = value;
  POWERMGNT.SetPowerCaliValues(values, PWR_COUNT);
  DBGLN("power calibration done %d, %d", index, value);
  hwTimer.resume();
}
#endif

void SendUIDOverMSP()
{
  MSPDataPackage[0] = MSP_ELRS_BIND;
  memcpy(&MSPDataPackage[1], &MasterUID[2], 4);
  BindingSendCount = 0;
  MspSender.ResetState();
  MspSender.SetDataToTransmit(MSPDataPackage, 5);
}

void EnterBindingMode()
{
  if (InBindingMode) {
      // Don't enter binding if we're already binding
      return;
  }

  // Disable the TX timer and wait for any TX to complete
  hwTimer.stop();
  while (busyTransmitting);

  // Queue up sending the Master UID as MSP packets
  SendUIDOverMSP();

  // Set UID to special binding values
  memcpy(UID, BindingUID, UID_LEN);

  OtaCrcInitializer = 0;
  OtaNonce = 0; // Lock the OtaNonce to prevent syncspam packets
  InBindingMode = true;

  // Start attempting to bind
  // Lock the RF rate and freq while binding
  SetRFLinkRate(enumRatetoIndex(RATE_BINDING));
  Radio.SetFrequencyReg(GetInitialFreq());
  if (isDualRadio() && config.GetAntennaMode() == TX_RADIO_MODE_GEMINI) // Gemini mode
  {
    Radio.SetFrequencyReg(FHSSgetInitialGeminiFreq(), SX12XX_Radio_2);
  }
  // Start transmitting again
  hwTimer.resume();

  DBGLN("Entered binding mode at freq = %d", Radio.currFreq);
}

void ExitBindingMode()
{
  if (!InBindingMode)
  {
    // Not in binding mode
    return;
  }

  MspSender.ResetState();

  // Reset UID to defined values
  memcpy(UID, MasterUID, UID_LEN);
  OtaUpdateCrcInitFromUid();

  InBindingMode = false;

  SetRFLinkRate(config.GetRate()); //return to original rate

  DBGLN("Exiting binding mode");
}

void ProcessMSPPacket(mspPacket_t *packet)
{
#if !defined(CRITICAL_FLASH)
  // Inspect packet for ELRS specific opcodes
  if (packet->function == MSP_ELRS_FUNC)
  {
    uint8_t opcode = packet->readByte();

    CHECK_PACKET_PARSING();

    switch (opcode)
    {
    case MSP_ELRS_POWER_CALI_GET:
      OnPowerGetCalibration(packet);
      break;
    case MSP_ELRS_POWER_CALI_SET:
      OnPowerSetCalibration(packet);
      break;
    default:
      break;
    }
  }
  else if (packet->function == MSP_SET_VTX_CONFIG)
  {
    if (packet->payload[0] < 48) // Standard 48 channel VTx table size e.g. A, B, E, F, R, L
    {
      config.SetVtxBand(packet->payload[0] / 8 + 1);
      config.SetVtxChannel(packet->payload[0] % 8);
    } else
    {
      return; // Packets containing frequency in MHz are not yet supported.
    }

    VtxTriggerSend();
  }
#endif
  if (packet->function == MSP_ELRS_GET_BACKPACK_VERSION)
  {
    memset(backpackVersion, 0, sizeof(backpackVersion));
    memcpy(backpackVersion, packet->payload, min((size_t)packet->payloadSize, sizeof(backpackVersion)-1));
  }
}

static void HandleUARTout()
{
  if (firmwareOptions.is_airport)
  {
    while (apOutputBuffer.size())
    {
      TxUSB->write(apOutputBuffer.pop());
    }
  }
}

static void setupSerial()
{  /*
   * Setup the logging/backpack serial port, and the USB serial port.
   * This is always done because we need a place to send data even if there is no backpack!
   */
  bool portConflict = false;
  int8_t rxPin = UNDEF_PIN;
  int8_t txPin = UNDEF_PIN;

  if (firmwareOptions.is_airport)
  {
    #if defined(PLATFORM_ESP32) || defined(PLATFORM_ESP8266)
      // Airport enabled - set TxUSB port to pins 1 and 3
      rxPin = 3;
      txPin = 1;

      if (GPIO_PIN_DEBUG_RX == rxPin && GPIO_PIN_DEBUG_TX == txPin)
      {
        // Avoid conflict between TxUSB and TxBackpack for UART0 (pins 1 and 3)
        // TxUSB takes priority over TxBackpack
        portConflict = true;
      }
    #else
      // For STM targets, assume GPIO_PIN_DEBUG defines point to USB
      rxPin = GPIO_PIN_DEBUG_RX;
      txPin = GPIO_PIN_DEBUG_TX;
    #endif
  }

// Setup TxBackpack
#if defined(PLATFORM_ESP32)
  Stream *serialPort;
  if (GPIO_PIN_DEBUG_RX != UNDEF_PIN && GPIO_PIN_DEBUG_TX != UNDEF_PIN && !portConflict)
  {
    serialPort = new HardwareSerial(2);
    ((HardwareSerial *)serialPort)->begin(BACKPACK_LOGGING_BAUD, SERIAL_8N1, GPIO_PIN_DEBUG_RX, GPIO_PIN_DEBUG_TX);
  }
  else
  {
    serialPort = new NullStream();
  }
#elif defined(PLATFORM_ESP8266)
  Stream *serialPort;
  if (GPIO_PIN_DEBUG_TX != UNDEF_PIN)
  {
    serialPort = new HardwareSerial(1);
    ((HardwareSerial*)serialPort)->begin(BACKPACK_LOGGING_BAUD, SERIAL_8N1, SERIAL_TX_ONLY, GPIO_PIN_DEBUG_TX);
  }
  else
  {
    serialPort = new NullStream();
  }
#elif defined(TARGET_TX_FM30)
  USBSerial *serialPort = &SerialUSB; // No way to disable creating SerialUSB global, so use it
  serialPort->begin();
#elif (defined(GPIO_PIN_DEBUG_RX) && GPIO_PIN_DEBUG_RX != UNDEF_PIN) || (defined(GPIO_PIN_DEBUG_TX) && GPIO_PIN_DEBUG_TX != UNDEF_PIN)
  HardwareSerial *serialPort = new HardwareSerial(2);
  #if defined(GPIO_PIN_DEBUG_RX) && GPIO_PIN_DEBUG_RX != UNDEF_PIN
    serialPort->setRx(GPIO_PIN_DEBUG_RX);
  #endif
  #if defined(GPIO_PIN_DEBUG_TX) && GPIO_PIN_DEBUG_TX != UNDEF_PIN
    serialPort->setTx(GPIO_PIN_DEBUG_TX);
  #endif
  serialPort->begin(BACKPACK_LOGGING_BAUD);
#else
  Stream *serialPort = new NullStream();
#endif
  TxBackpack = serialPort;

// Setup TxUSB
#if defined(PLATFORM_ESP32)
  if (rxPin != UNDEF_PIN && txPin != UNDEF_PIN)
  {
    TxUSB = new HardwareSerial(1);
    ((HardwareSerial *)TxUSB)->begin(firmwareOptions.uart_baud, SERIAL_8N1, rxPin, txPin);
  }
  else
  {
    TxUSB = new NullStream();
  }
#else
  TxUSB = new NullStream();
#endif
}

/**
 * Target-specific initialization code called early in setup()
 * Setup GPIOs or other hardware, config not yet loaded
 ***/
static void setupTarget()
{
#if defined(TARGET_TX_FM30)
  pinMode(GPIO_PIN_UART3RX_INVERT, OUTPUT); // RX3 inverter (from radio)
  digitalWrite(GPIO_PIN_UART3RX_INVERT, LOW); // RX3 not inverted
  pinMode(GPIO_PIN_BLUETOOTH_EN, OUTPUT); // Bluetooth enable (disabled)
  digitalWrite(GPIO_PIN_BLUETOOTH_EN, HIGH);
  pinMode(GPIO_PIN_UART1RX_INVERT, OUTPUT); // RX1 inverter (TX handled in CRSF)
  digitalWrite(GPIO_PIN_UART1RX_INVERT, HIGH);
  pinMode(GPIO_PIN_ANT_CTRL_FIXED, OUTPUT);
  digitalWrite(GPIO_PIN_ANT_CTRL_FIXED, LOW); // LEFT antenna
  HardwareSerial *uart2 = new HardwareSerial(USART2);
  uart2->begin(57600);
  CRSF::PortSecondary = uart2;
#endif

#if defined(TARGET_TX_FM30_MINI)
  pinMode(GPIO_PIN_UART1TX_INVERT, OUTPUT); // TX1 inverter used for debug
  digitalWrite(GPIO_PIN_UART1TX_INVERT, LOW);
#endif

  if (GPIO_PIN_ANT_CTRL != UNDEF_PIN)
  {
    pinMode(GPIO_PIN_ANT_CTRL, OUTPUT);
    digitalWrite(GPIO_PIN_ANT_CTRL, diversityAntennaState);
  }
  if (GPIO_PIN_ANT_CTRL_COMPL != UNDEF_PIN)
  {
    pinMode(GPIO_PIN_ANT_CTRL_COMPL, OUTPUT);
    digitalWrite(GPIO_PIN_ANT_CTRL_COMPL, !diversityAntennaState);
  }

  setupTargetCommon();
  setupSerial();
}

bool setupHardwareFromOptions()
{
#if defined(TARGET_UNIFIED_TX)
  // Setup default logging in case of failure, or no layout
  Serial.begin(115200);
  TxBackpack = &Serial;

  if (!options_init())
  {
    // Register the WiFi with the framework
    static device_affinity_t wifi_device[] = {
        {&WIFI_device, 1}
    };
    devicesRegister(wifi_device, ARRAY_SIZE(wifi_device));
    devicesInit();

    connectionState = hardwareUndefined;
    return false;
  }
#endif

  return true;
}

static void cyclePower()
{
  // Only change power if we are running normally
  if (connectionState < MODE_STATES)
  {
    PowerLevels_e curr = POWERMGNT::currPower();
    if (curr == POWERMGNT::getMaxPower())
    {
      POWERMGNT::setPower(POWERMGNT::getMinPower());
    }
    else
    {
      POWERMGNT::incPower();
    }
  }
}

void setup()
{
  if (setupHardwareFromOptions())
  {
    initUID();
    setupTarget();
    // Register the devices with the framework
    devicesRegister(ui_devices, ARRAY_SIZE(ui_devices));
    // Initialise the devices
    devicesInit();
    DBGLN("Initialised devices");

    FHSSrandomiseFHSSsequence(uidMacSeedGet());

    Radio.RXdoneCallback = &RXdoneISR;
    Radio.TXdoneCallback = &TXdoneISR;

    crsf.connected = &UARTconnected; // it will auto init when it detects UART connection
    if (!firmwareOptions.is_airport)
    {
      crsf.disconnected = &UARTdisconnected;
    }
    crsf.RecvModelUpdate = &ModelUpdateReq;
    hwTimer.callbackTock = &timerCallbackNormal;
    DBGLN("ExpressLRS TX Module Booted...");

    eeprom.Begin(); // Init the eeprom
    config.SetStorageProvider(&eeprom); // Pass pointer to the Config class for access to storage
    config.Load(); // Load the stored values from eeprom

    Radio.currFreq = GetInitialFreq(); //set frequency first or an error will occur!!!
    #if defined(RADIO_SX127X)
    //Radio.currSyncWord = UID[3];
    #endif
    bool init_success;
    #if defined(USE_BLE_JOYSTICK)
    init_success = true; // No radio is attached with a joystick only module.  So we are going to fake success so that crsf, hwTimer etc are initiated below.
    #else
    if (GPIO_PIN_SCK != UNDEF_PIN)
    {
      init_success = Radio.Begin();
    }
    else
    {
      // Assume BLE Joystick mode if no radio SCK pin
      init_success = true;
    }
    #endif

    if (!init_success)
    {
      connectionState = radioFailed;
    }
    else
    {
      TelemetryReceiver.SetDataToReceive(CRSFinBuffer, sizeof(CRSFinBuffer));

      POWERMGNT.init();
      DynamicPower_Init();

      // Set the pkt rate, TLM ratio, and power from the stored eeprom values
      ChangeRadioParams();

  #if defined(Regulatory_Domain_EU_CE_2400)
      BeginClearChannelAssessment();
  #endif
      hwTimer.init();
      connectionState = noCrossfire;
    }
  }

#if defined(HAS_BUTTON)
  registerButtonFunction(ACTION_BIND, EnterBindingMode);
  registerButtonFunction(ACTION_INCREASE_POWER, cyclePower);
#endif

  devicesStart();

  if (firmwareOptions.is_airport)
  {
    config.SetTlm(TLM_RATIO_1_2); // Force TLM ratio of 1:2 for balanced bi-dir link
    config.SetMotionMode(0); // Ensure motion detection is off
    UARTconnected();
  }
}

void loop()
{
  uint32_t now = millis();

  HandleUARTout(); // Only used for non-CRSF output

  #if defined(USE_BLE_JOYSTICK)
  if (connectionState != bleJoystick && connectionState != noCrossfire) // Wait until the correct crsf baud has been found
  {
      connectionState = bleJoystick;
  }
  #endif

  if (connectionState < MODE_STATES)
  {
    UpdateConnectDisconnectStatus();
  }

  // Update UI devices
  devicesUpdate(now);

  // Not a device because it must be run on the loop core
  checkBackpackUpdate();

  #if defined(PLATFORM_ESP8266) || defined(PLATFORM_ESP32)
    // If the reboot time is set and the current time is past the reboot time then reboot.
    if (rebootTime != 0 && now > rebootTime) {
      ESP.restart();
    }
  #endif

  executeDeferredFunction(now);

  if (firmwareOptions.is_airport && apInputBuffer.size() < AP_MAX_BUF_LEN && connectionState == connected && TxUSB->available())
  {
    apInputBuffer.push(TxUSB->read());
  }

  if (TxBackpack->available())
  {
    if (msp.processReceivedByte(TxBackpack->read()))
    {
      // Finished processing a complete packet
      ProcessMSPPacket(msp.getReceivedPacket());
      msp.markPacketReceived();
    }
  }

  if (connectionState > MODE_STATES)
  {
    return;
  }

  CheckReadyToSend();
  CheckConfigChangePending();
  DynamicPower_Update(now);
  VtxPitmodeSwitchUpdate();

  /* Send TLM updates to handset if connected + reporting period
   * is elapsed. This keeps handset happy dispite of the telemetry ratio */
  if ((connectionState == connected) && (LastTLMpacketRecvMillis != 0) &&
      (now >= (uint32_t)(firmwareOptions.tlm_report_interval + TLMpacketReported))) {
    crsf.sendLinkStatisticsToTX();
    TLMpacketReported = now;
  }

  if (TelemetryReceiver.HasFinishedData())
  {
      crsf.sendTelemetryToTX(CRSFinBuffer);
      TelemetryReceiver.Unlock();
  }

  // only send msp data when binding is not active
  static bool mspTransferActive = false;
  if (InBindingMode)
  {
    // exit bind mode if package after some repeats
    if (BindingSendCount > 6) {
      ExitBindingMode();
    }
  }
  else if (!MspSender.IsActive())
  {
    // sending is done and we need to update our flag
    if (mspTransferActive)
    {
      // unlock buffer for msp messages
      crsf.UnlockMspMessage();
      mspTransferActive = false;
    }
    // we are not sending so look for next msp package
    else
    {
      uint8_t* mspData;
      uint8_t mspLen;
      crsf.GetMspMessage(&mspData, &mspLen);
      // if we have a new msp package start sending
      if (mspData != nullptr)
      {
        MspSender.SetDataToTransmit(mspData, mspLen);
        mspTransferActive = true;
      }
    }
  }
}
