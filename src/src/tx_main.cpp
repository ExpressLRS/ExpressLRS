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

#ifndef TLM_REPORT_INTERVAL_MS
#define TLM_REPORT_INTERVAL_MS 320LU // Default to 320ms
#endif

/// define some libs to use ///
hwTimer hwTimer;
GENERIC_CRC14 ota_crc(ELRS_CRC14_POLY);
CRSF crsf;
POWERMGNT POWERMGNT;
MSP msp;
ELRS_EEPROM eeprom;
TxConfig config;
Stream *LoggingBackpack;

volatile uint8_t NonceTX;

#if defined(PLATFORM_ESP8266) || defined(PLATFORM_ESP32)
unsigned long rebootTime = 0;
extern bool webserverPreventAutoStart;
#endif
//// MSP Data Handling ///////
bool NextPacketIsMspData = false;  // if true the next packet will contain the msp data

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
StubbornReceiver TelemetryReceiver(ELRS_TELEMETRY_MAX_PACKAGES);
StubbornSender MspSender(ELRS_MSP_MAX_PACKAGES);
uint8_t CRSFinBuffer[CRSF_MAX_PACKET_LEN+1];

device_affinity_t ui_devices[] = {
  {&CRSF_device, 0},
#ifdef HAS_LED
  {&LED_device, 1},
#endif
#ifdef HAS_RGB
  {&RGB_device, 1},
#endif
  {&LUA_device, 1},
#if defined(USE_TX_BACKPACK)
  {&Backpack_device, 1},
#endif
#ifdef HAS_BLE
  {&BLE_device, 1},
#endif
#ifdef HAS_BUZZER
  {&Buzzer_device, 1},
#endif
#ifdef HAS_WIFI
  {&WIFI_device, 1},
#endif
#ifdef HAS_BUTTON
  {&Button_device, 1},
#endif
#if defined(HAS_TFT_SCREEN) || defined(USE_OLED_SPI) || defined(USE_OLED_SPI_SMALL) || defined(USE_OLED_I2C)
  {&Screen_device, 0},
#endif
#ifdef HAS_GSENSOR
  {&Gsensor_device, 0},
#endif
#if defined(HAS_THERMAL) || defined(HAS_FAN)
  {&Thermal_device, 0},
#endif
#if defined(GPIO_PIN_PA_PDET) && GPIO_PIN_PA_PDET != UNDEF_PIN
  {&PDET_device, 1},
#endif
  {&VTX_device, 1}
};

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

bool ICACHE_RAM_ATTR IsArmed()
{
   return CRSF_to_BIT(crsf.ChannelDataIn[AUX1]);
}

void ICACHE_RAM_ATTR ProcessTLMpacket(SX12xxDriverCommon::rx_status const status)
{
  if (status != SX12xxDriverCommon::SX12XX_RX_OK)
  {
    DBGLN("TLM HW CRC error");
    return;
  }
  uint16_t const inCRC = (((uint16_t)Radio.RXdataBuffer[0] & 0b11111100) << 6) | Radio.RXdataBuffer[7];

  Radio.RXdataBuffer[0] &= 0b11;
  uint16_t const calculatedCRC = ota_crc.calc(Radio.RXdataBuffer, 7, CRCInitializer);

  uint8_t const type = Radio.RXdataBuffer[0] & TLM_PACKET;
  uint8_t const TLMheader = Radio.RXdataBuffer[1];

  if ((inCRC != calculatedCRC))
  {
    DBGLN("TLM crc error");
    return;
  }

  if (type != TLM_PACKET)
  {
    DBGLN("TLM type error %d", type);
    return;
  }

  LastTLMpacketRecvMillis = millis();
  LQCalc.add();

    switch(TLMheader & ELRS_TELEMETRY_TYPE_MASK)
    {
        case ELRS_TELEMETRY_TYPE_LINK:
            // Antenna is the high bit in the RSSI_1 value
            // RSSI received is signed, inverted polarity (positive value = -dBm)
            // OpenTX's value is signed and will display +dBm and -dBm properly
            crsf.LinkStatistics.uplink_RSSI_1 = -(Radio.RXdataBuffer[2] & 0x7f);
            crsf.LinkStatistics.uplink_RSSI_2 = -(Radio.RXdataBuffer[3] & 0x7f);
            crsf.LinkStatistics.uplink_SNR = Radio.RXdataBuffer[4];
            crsf.LinkStatistics.uplink_Link_quality = Radio.RXdataBuffer[5];
            crsf.LinkStatistics.downlink_SNR = Radio.LastPacketSNR;
            crsf.LinkStatistics.downlink_RSSI = Radio.LastPacketRSSI;
            crsf.LinkStatistics.active_antenna = Radio.RXdataBuffer[2] >> 7;
            connectionHasModelMatch = Radio.RXdataBuffer[3] >> 7;
            // -- uplink_TX_Power is updated via devCRSF event, so it updates with no telemetry
            // -- rf_mode is updated when we change rates
            // -- downlink_Link_quality is updated before the LQ period is incremented
            MspSender.ConfirmCurrentPayload(Radio.RXdataBuffer[6] == 1);

            DynamicPower_TelemetryUpdate(dptuNewLinkstats);
            break;

        case ELRS_TELEMETRY_TYPE_DATA:
            TelemetryReceiver.ReceiveData(TLMheader >> ELRS_TELEMETRY_SHIFT, Radio.RXdataBuffer + 2);
            break;
    }
}

void ICACHE_RAM_ATTR GenerateSyncPacketData()
{
  const uint8_t SwitchEncMode = config.GetSwitchMode();
  uint8_t Index;
  if (syncSpamCounter)
  {
    Index = config.GetRate();
  }
  else
  {
    Index = ExpressLRS_currAirRate_Modparams->index;
  }

  if (syncSpamCounter)
    --syncSpamCounter;
  SyncPacketLastSent = millis();

  // TLM ratio is boosted for one sync cycle when the MspSender goes active
  expresslrs_tlm_ratio_e newTlmRatio = (MspSender.IsActive()) ? TLM_RATIO_1_2 : (expresslrs_tlm_ratio_e)config.GetTlm();
  // Delay going into disconnected state when the TLM ratio increases
  if (connectionState == connected && ExpressLRS_currAirRate_Modparams->TLMinterval < newTlmRatio)
    LastTLMpacketRecvMillis = SyncPacketLastSent;
  ExpressLRS_currAirRate_Modparams->TLMinterval = newTlmRatio;

  Radio.TXdataBuffer[0] = SYNC_PACKET & 0b11;
  Radio.TXdataBuffer[1] = FHSSgetCurrIndex();
  Radio.TXdataBuffer[2] = NonceTX;
  Radio.TXdataBuffer[3] = ((Index & SYNC_PACKET_RATE_MASK) << SYNC_PACKET_RATE_OFFSET) +
                          ((newTlmRatio & SYNC_PACKET_TLM_MASK) << SYNC_PACKET_TLM_OFFSET) +
                          ((SwitchEncMode & SYNC_PACKET_SWITCH_MASK) << SYNC_PACKET_SWITCH_OFFSET);
  Radio.TXdataBuffer[4] = UID[3];
  Radio.TXdataBuffer[5] = UID[4];
  Radio.TXdataBuffer[6] = UID[5];
  // For model match, the last byte of the binding ID is XORed with the inverse of the modelId
  if (!InBindingMode && config.GetModelMatch())
  {
    Radio.TXdataBuffer[6] ^= (~crsf.getModelID()) & MODELMATCH_MASK;
  }
}

uint8_t adjustPacketRateForBaud(uint8_t rateIndex)
{
  #if defined(RADIO_SX128X)
    // Packet rate limited to 250Hz if we are on 115k baud
    if (crsf.GetCurrentBaudRate() == 115200) {
      while (rateIndex < RATE_MAX) {
        expresslrs_mod_settings_s const * const ModParams = get_elrs_airRateConfig(rateIndex);
        if (ModParams->enum_rate <= RATE_LORA_250HZ) {
          break;
        }
        rateIndex++;
      }
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
  if ((ModParams == ExpressLRS_currAirRate_Modparams)
    && (RFperf == ExpressLRS_currAirRate_RFperfParams)
    && (invertIQ == Radio.IQinverted))
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
               , uidMacSeedGet(), CRCInitializer, (ModParams->radio_type == RADIO_TYPE_SX128x_FLRC)
#endif
               );

  ExpressLRS_currAirRate_Modparams = ModParams;
  ExpressLRS_currAirRate_RFperfParams = RFperf;
  crsf.LinkStatistics.rf_Mode = ModParams->enum_rate;

  crsf.setSyncParams(interval);
  connectionState = disconnected;
  rfModeLastChangedMS = millis();
}

void ICACHE_RAM_ATTR HandleFHSS()
{
  uint8_t modresult = (NonceTX + 1) % ExpressLRS_currAirRate_Modparams->FHSShopInterval;
  // If the next packet should be on the next FHSS frequency, do the hop
  if (!InBindingMode && modresult == 0)
  {
    Radio.SetFrequencyReg(FHSSgetNextFreq());
  }
}

void ICACHE_RAM_ATTR HandlePrepareForTLM()
{
  uint8_t modresult = (NonceTX + 1) % TLMratioEnumToValue(ExpressLRS_currAirRate_Modparams->TLMinterval);
  // If next packet is going to be telemetry, start listening to have a large receive window (time-wise)
  if (ExpressLRS_currAirRate_Modparams->TLMinterval != TLM_RATIO_NO_TLM && modresult == 0)
  {
    Radio.RXnb();
    TelemetryRcvPhase = ttrpPreReceiveGap;
  }
}

void ICACHE_RAM_ATTR SendRCdataToRF()
{
  uint32_t now = millis();
  static uint8_t syncSlot;
#if defined(NO_SYNC_ON_ARM)
  uint32_t SyncInterval = 250;
  bool skipSync = IsArmed() || InBindingMode;
#else
  uint32_t SyncInterval = (connectionState == connected) ? ExpressLRS_currAirRate_RFperfParams->SyncPktIntervalConnected : ExpressLRS_currAirRate_RFperfParams->SyncPktIntervalDisconnected;
  bool skipSync = InBindingMode;
#endif

  uint8_t NonceFHSSresult = NonceTX % ExpressLRS_currAirRate_Modparams->FHSShopInterval;
  bool WithinSyncSpamResidualWindow = now - rfModeLastChangedMS < syncSpamAResidualTimeMS;

  // Sync spam only happens on slot 1 and 2 and can't be disabled
  if ((syncSpamCounter || WithinSyncSpamResidualWindow) && (NonceFHSSresult == 1 || NonceFHSSresult == 2))
  {
    GenerateSyncPacketData();
    syncSlot = 0; // reset the sync slot in case the new rate (after the syncspam) has a lower FHSShopInterval
  }
  // Regular sync rotates through 4x slots, twice on each slot, and telemetry pushes it to the next slot up
  // But only on the sync FHSS channel and with a timed delay between them
  else if ((!skipSync) && ((syncSlot / 2) <= NonceFHSSresult) && (now - SyncPacketLastSent > SyncInterval) && (Radio.currFreq == GetInitialFreq()))
  {
    GenerateSyncPacketData();
    syncSlot = (syncSlot + 1) % (ExpressLRS_currAirRate_Modparams->FHSShopInterval * 2);
  }
  else
  {
    if (NextPacketIsMspData && MspSender.IsActive())
    {
      uint8_t *data;
      uint8_t maxLength;
      uint8_t packageIndex;
      MspSender.GetCurrentPayload(&packageIndex, &maxLength, &data);
      Radio.TXdataBuffer[0] = MSP_DATA_PACKET & 0b11;
      Radio.TXdataBuffer[1] = packageIndex;
      Radio.TXdataBuffer[2] = maxLength > 0 ? *data : 0;
      Radio.TXdataBuffer[3] = maxLength >= 1 ? *(data + 1) : 0;
      Radio.TXdataBuffer[4] = maxLength >= 2 ? *(data + 2) : 0;
      Radio.TXdataBuffer[5] = maxLength >= 3 ? *(data + 3): 0;
      Radio.TXdataBuffer[6] = maxLength >= 4 ? *(data + 4): 0;
      // send channel data next so the channel messages also get sent during msp transmissions
      NextPacketIsMspData = false;
      // counter can be increased even for normal msp messages since it's reset if a real bind message should be sent
      BindingSendCount++;
      // If the telemetry ratio isn't already 1:2, send a sync packet to boost it
      // to add bandwidth for the reply
      if (ExpressLRS_currAirRate_Modparams->TLMinterval != TLM_RATIO_1_2)
        syncSpamCounter = 1;
    }
    else
    {
      // always enable msp after a channel package since the slot is only used if MspSender has data to send
      NextPacketIsMspData = true;
      PackChannelData(Radio.TXdataBuffer, &crsf, TelemetryReceiver.GetCurrentConfirm(),
        NonceTX, TLMratioEnumToValue(ExpressLRS_currAirRate_Modparams->TLMinterval));
    }
  }

  // artificially inject the low bits of the nonce on data packets, this will be overwritten with the CRC after it's calculated
  if (Radio.TXdataBuffer[0] == RC_DATA_PACKET && OtaSwitchModeCurrent == smHybridWide)
    Radio.TXdataBuffer[0] |= NonceFHSSresult << 2;

  ///// Next, Calculate the CRC and put it into the buffer /////
  uint16_t crc = ota_crc.calc(Radio.TXdataBuffer, 7, CRCInitializer);
  Radio.TXdataBuffer[0] = (Radio.TXdataBuffer[0] & 0b11) | ((crc >> 6) & 0b11111100);
  Radio.TXdataBuffer[7] = crc & 0xFF;

#if defined(Regulatory_Domain_EU_CE_2400)
  if (ChannelIsClear())
#endif
  {
    Radio.TXnb();
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
  crsf.JustSentRFpacket();

  // Nonce advances on every timer tick
  if (!InBindingMode)
    NonceTX++;

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
    // Indicate no telemetry packet received with -1
    if (!LQCalc.currentIsSet())
      DynamicPower_TelemetryUpdate(dptuMissed);
    LQCalc.inc();
    return;
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
  NonceTX++;
  if ((NonceTX + 1) % ExpressLRS_currAirRate_Modparams->FHSShopInterval == 0)
    ++FHSSptr;
}

void UARTdisconnected()
{
  hwTimer.stop();
  connectionState = noCrossfire;
}

void UARTconnected()
{
  #if defined(PLATFORM_ESP32) || defined(PLATFORM_ESP8266)
  webserverPreventAutoStart = true;
  #endif
  rfModeLastChangedMS = millis(); // force syncspam on first packets
  SetRFLinkRate(config.GetRate());
  if (connectionState == noCrossfire || connectionState < MODE_STATES)
  {
    connectionState = disconnected; // set here because SetRFLinkRate may have early exited and not set the state
  }
  hwTimer.resume();
}

static void ChangeRadioParams()
{
  ModelUpdatePending = false;

  SetRFLinkRate(config.GetRate());
  OtaSetSwitchMode((OtaSwitchMode_e)config.GetSwitchMode());
  // Dynamic Power starts at MinPower and will boost if switch is set or IsArmed and disconnected
  POWERMGNT.setPower(config.GetDynamicPower() ? MinPower : (PowerLevels_e)config.GetPower());
  // TLM interval is set on the next SYNC packet
#if defined(Regulatory_Domain_EU_CE_2400)
  LBTEnabled = (config.GetPower() > PWR_10mW);
#endif
}

void ICACHE_RAM_ATTR ModelUpdateReq()
{
  // There's a near 100% chance we started up transmitting at Model 0's
  // rate before we got the set modelid command from the handset, so do the
  // normal way of switching rates with syncspam first (but only if changing)
  if (config.SetModelId(crsf.getModelID()))
  {
    syncSpamCounter = syncSpamAmount;
    ModelUpdatePending = true;
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

void ICACHE_RAM_ATTR RXdoneISR(SX12xxDriverCommon::rx_status const status)
{
  ProcessTLMpacket(status);
  busyTransmitting = false;
}

void ICACHE_RAM_ATTR TXdoneISR()
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
  busyTransmitting = false;
}

static void UpdateConnectDisconnectStatus()
{
  // Number of telemetry packets which can be lost in a row before going to disconnected state
  constexpr unsigned RX_LOSS_CNT = 5;
  const uint32_t tlmInterval = TLMratioEnumToValue(ExpressLRS_currAirRate_Modparams->TLMinterval);
  // +2 to account for any rounding down and partial millis()
  const uint32_t msConnectionLostTimeout = tlmInterval * ExpressLRS_currAirRate_Modparams->interval / (1000U / RX_LOSS_CNT) + 2;
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
    }
  }
  else
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
  MspSender.SetDataToTransmit(1, MSPDataPackage, ELRS_MSP_BYTES_PER_CALL);
}

static void CheckReadyToSend()
{
  if (RxWiFiReadyToSend)
  {
    RxWiFiReadyToSend = false;
    if (!IsArmed())
    {
      SendRxWiFiOverMSP();
    }
  }
}

void OnRFModePacket(mspPacket_t *packet)
{
  // Parse the RF mode
  uint8_t rfMode = packet->readByte();
  CHECK_PACKET_PARSING();

  switch (rfMode)
  {
  case RATE_LORA_200HZ:
  case RATE_LORA_100HZ:
  case RATE_LORA_50HZ:
    SetRFLinkRate(enumRatetoIndex((expresslrs_RFrates_e)rfMode));
    break;
  default:
    // Unsupported rate requested
    break;
  }
}

void OnTxPowerPacket(mspPacket_t *packet)
{
  // Parse the TX power
  uint8_t txPower = packet->readByte();
  CHECK_PACKET_PARSING();
  DBGLN("TX setpower");

  if (txPower < PWR_COUNT)
    POWERMGNT.setPower((PowerLevels_e)txPower);
}

void OnTLMRatePacket(mspPacket_t *packet)
{
  // Parse the TLM rate
  // uint8_t tlmRate = packet->readByte();
  CHECK_PACKET_PARSING();

  // TODO: Implement dynamic TLM rates
  // switch (tlmRate) {
  // case TLM_RATIO_NO_TLM:
  //   break;
  // case TLM_RATIO_1_128:
  //   break;
  // default:
  //   // Unsupported rate requested
  //   break;
  // }
}

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


void SendUIDOverMSP()
{
  MSPDataPackage[0] = MSP_ELRS_BIND;
  memcpy(&MSPDataPackage[1], &MasterUID[2], 4);
  BindingSendCount = 0;
  MspSender.SetDataToTransmit(5, MSPDataPackage, ELRS_MSP_BYTES_PER_CALL);
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

  CRCInitializer = 0;
  NonceTX = 0; // Lock the NonceTX to prevent syncspam packets
  InBindingMode = true;

  // Start attempting to bind
  // Lock the RF rate and freq while binding
  SetRFLinkRate(RATE_BINDING);
  Radio.SetFrequencyReg(GetInitialFreq());
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

  // Reset UID to defined values
  memcpy(UID, MasterUID, UID_LEN);

  CRCInitializer = (UID[4] << 8) | UID[5];

  InBindingMode = false;

  SetRFLinkRate(config.GetRate()); //return to original rate

  DBGLN("Exiting binding mode");
}

void ProcessMSPPacket(mspPacket_t *packet)
{
  // Inspect packet for ELRS specific opcodes
  if (packet->function == MSP_ELRS_FUNC)
  {
    uint8_t opcode = packet->readByte();

    CHECK_PACKET_PARSING();

    switch (opcode)
    {
    case MSP_ELRS_RF_MODE:
      OnRFModePacket(packet);
      break;
    case MSP_ELRS_TX_PWR:
      OnTxPowerPacket(packet);
      break;
    case MSP_ELRS_TLM_RATE:
      OnTLMRatePacket(packet);
      break;
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
}

static void setupLoggingBackpack()
{  /*
   * Setup the logging/backpack serial port.
   * This is always done because we need a place to send data even if there is no backpack!
   */
#if defined(PLATFORM_ESP32) && defined(GPIO_PIN_DEBUG_RX) && GPIO_PIN_DEBUG_RX != UNDEF_PIN && defined(GPIO_PIN_DEBUG_TX) && GPIO_PIN_DEBUG_TX != UNDEF_PIN
  HardwareSerial *serialPort = new HardwareSerial(2);
  serialPort->begin(BACKPACK_LOGGING_BAUD, SERIAL_8N1, GPIO_PIN_DEBUG_RX, GPIO_PIN_DEBUG_TX);
#elif defined(PLATFORM_ESP8266) && defined(GPIO_PIN_DEBUG_TX) && GPIO_PIN_DEBUG_TX != UNDEF_PIN
  HardwareSerial *serialPort = new HardwareSerial(0);
  serialPort->begin(BACKPACK_LOGGING_BAUD, SERIAL_8N1, SERIAL_TX_ONLY, GPIO_PIN_DEBUG_TX);
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
  LoggingBackpack = serialPort;
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
  pinMode(GPIO_PIN_ANT_CTRL, OUTPUT);
  digitalWrite(GPIO_PIN_ANT_CTRL, LOW); // LEFT antenna
  HardwareSerial *uart2 = new HardwareSerial(USART2);
  uart2->begin(57600);
  CRSF::PortSecondary = uart2;
#endif

#if defined(TARGET_TX_FM30_MINI)
  pinMode(GPIO_PIN_UART1TX_INVERT, OUTPUT); // TX1 inverter used for debug
  digitalWrite(GPIO_PIN_UART1TX_INVERT, LOW);
  pinMode(GPIO_PIN_ANT_CTRL, OUTPUT);
  digitalWrite(GPIO_PIN_ANT_CTRL, LOW); // LEFT antenna
#endif

  setupTargetCommon();
  setupLoggingBackpack();
}

void setup()
{
  setupTarget();
  // Register the devices with the framework
  devicesRegister(ui_devices, ARRAY_SIZE(ui_devices));
  // Initialise the devices
  devicesInit();

  FHSSrandomiseFHSSsequence(uidMacSeedGet());

  Radio.RXdoneCallback = &RXdoneISR;
  Radio.TXdoneCallback = &TXdoneISR;

  crsf.connected = &UARTconnected; // it will auto init when it detects UART connection
  crsf.disconnected = &UARTdisconnected;
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
  bool init_success = Radio.Begin();

  #if defined(USE_BLE_JOYSTICK)
    init_success = true; // No radio is attached with a joystick only module.  So we are going to fake success so that crsf, hwTimer etc are initiated below.
  #endif

  if (!init_success)
  {
    connectionState = radioFailed;
  }
  else
  {
    TelemetryReceiver.SetDataToReceive(sizeof(CRSFinBuffer), CRSFinBuffer, ELRS_TELEMETRY_BYTES_PER_CALL);

    POWERMGNT.init();

    // Set the pkt rate, TLM ratio, and power from the stored eeprom values
    ChangeRadioParams();
    DynamicPower_Init();

#if defined(Regulatory_Domain_EU_CE_2400)
    BeginClearChannelAssessment();
#endif
    hwTimer.init();
    connectionState = noCrossfire;
  }

  devicesStart();
}

void loop()
{
  uint32_t now = millis();

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

  #if defined(PLATFORM_ESP8266) || defined(PLATFORM_ESP32)
    // If the reboot time is set and the current time is past the reboot time then reboot.
    if (rebootTime != 0 && now > rebootTime) {
      ESP.restart();
    }
  #endif

  if (connectionState > MODE_STATES)
  {
    return;
  }

  CheckReadyToSend();
  CheckConfigChangePending();
  DynamicPower_Update(now);
  VtxPitmodeSwitchUpdate();

  if (LoggingBackpack->available())
  {
    if (msp.processReceivedByte(LoggingBackpack->read()))
    {
      // Finished processing a complete packet
      ProcessMSPPacket(msp.getReceivedPacket());
      msp.markPacketReceived();
    }
  }

  /* Send TLM updates to handset if connected + reporting period
   * is elapsed. This keeps handset happy dispite of the telemetry ratio */
  if ((connectionState == connected) && (LastTLMpacketRecvMillis != 0) &&
      (now >= (uint32_t)(TLM_REPORT_INTERVAL_MS + TLMpacketReported))) {
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
        MspSender.SetDataToTransmit(mspLen, mspData, ELRS_MSP_BYTES_PER_CALL);
        mspTransferActive = true;
      }
    }
  }
}
