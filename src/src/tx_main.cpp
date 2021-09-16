#include "targets.h"
#include "common.h"

#if defined(Regulatory_Domain_AU_915) || defined(Regulatory_Domain_EU_868) || defined(Regulatory_Domain_IN_866) || defined(Regulatory_Domain_FCC_915) || defined(Regulatory_Domain_AU_433) || defined(Regulatory_Domain_EU_433)
#include "SX127xDriver.h"
SX127xDriver Radio;
#elif defined(Regulatory_Domain_ISM_2400)
#include "SX1280Driver.h"
SX1280Driver Radio;
#endif

#include "CRSF.h"
#include "lua.h"
#include "luaParams.h"

#include "FHSS.h"
#include "logging.h"
#include "POWERMGNT.h"
#include "LED.h"
#include "msp.h"
#include "msptypes.h"
#include <OTA.h>
#include "elrs_eeprom.h"
#include "options.h"
#include "config.h"
#include "hwTimer.h"
#include "LQCALC.h"
#include "telemetry_protocol.h"
#include "stubborn_receiver.h"
#include "stubborn_sender.h"

#ifdef HAS_OLED
#include "OLED.h"
#endif

#ifdef PLATFORM_ESP8266
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#endif
#ifdef PLATFORM_ESP32
#include "ESP32_WebUpdate.h"
#endif
#include "ESP32_BLE_HID.h"
volatile bool BLEjoystickRefresh = false;

#if defined(GPIO_PIN_BUTTON) && (GPIO_PIN_BUTTON != UNDEF_PIN)
#include "button.h"
Button<GPIO_PIN_BUTTON, GPIO_BUTTON_INVERTED> button;
#endif

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

bool crsfSeen = false;

#if defined(HAS_OLED)
OLED OLED;
char commitStr[7] = {LATEST_COMMIT , 0};
#endif

volatile uint8_t NonceTX;

#ifdef PLATFORM_ESP32
unsigned long rebootTime = 0;
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

LQCALC<10> LQCalc;

volatile bool busyTransmitting;
static volatile bool ModelUpdatePending;

char luaBadGoodString[10] = {"xxxxx/yyy"};

bool WaitRXresponse = false;

bool InBindingMode = false;
uint8_t BindingPackage[5];
uint8_t BindingSendCount = 0;
void EnterBindingMode();
void ExitBindingMode();
void SendUIDOverMSP();
void VtxConfigToMSPOut();
void eepromWriteToMSPOut();
uint8_t VtxConfigReadyToSend = false;

StubbornReceiver TelemetryReceiver(ELRS_TELEMETRY_MAX_PACKAGES);
StubbornSender MspSender(ELRS_MSP_MAX_PACKAGES);
uint8_t CRSFinBuffer[CRSF_MAX_PACKET_LEN+1];
// MSP packet handling function defs
void ProcessMSPPacket(mspPacket_t *packet);
void OnRFModePacket(mspPacket_t *packet);
void OnTxPowerPacket(mspPacket_t *packet);
void OnTLMRatePacket(mspPacket_t *packet);

uint8_t baseMac[6];

//////////// DYNAMIC TX OUTPUT POWER ////////////

#if !defined(DYNPOWER_THRESH_UP)
  #define DYNPOWER_THRESH_UP              15
#endif
#if !defined(DYNPOWER_THRESH_DN)
  #define DYNPOWER_THRESH_DN              21
#endif
#define DYNAMIC_POWER_MIN_RECORD_NUM       5 // average at least this number of records
#define DYNAMIC_POWER_BOOST_LQ_THRESHOLD  20 // If LQ is dropped suddenly for this amount (relative), immediately boost to the max power configured.
#define DYNAMIC_POWER_BOOST_LQ_MIN        50 // If LQ is below this value (absolute), immediately boost to the max power configured.
#define DYNAMIC_POWER_MOVING_AVG_K         8 // Number of previous values for calculating moving average. Best with power of 2.
static int32_t dynamic_power_rssi_sum;
static int32_t dynamic_power_rssi_n;
static int32_t dynamic_power_avg_lq;
static bool dynamic_power_updated;

static bool ICACHE_RAM_ATTR IsArmed()
{
   return CRSF_to_BIT(crsf.ChannelDataIn[AUX1]);
}

//////////// DYNAMIC TX OUTPUT POWER ////////////

// Assume this function is called inside loop(). Heavy functions goes here.
void DynamicPower_Update()
{
  if (!config.GetDynamicPower()) {
    return;
  }

  // =============  DYNAMIC_POWER_BOOST: Switch-triggered power boost up ==============
  // Or if telemetry is lost while armed (done up here because dynamic_power_updated is only updated on telemetry)
  uint8_t boostChannel = config.GetBoostChannel();
  if ((connectionState == disconnected && IsArmed()) ||
    (boostChannel && (CRSF_to_BIT(crsf.ChannelDataIn[AUX9 + boostChannel - 1]) == 0)))
  {
    POWERMGNT.setPower((PowerLevels_e)config.GetPower());
    // POWERMGNT.setPower((PowerLevels_e)MaxPower);    // if you want to make the power to the aboslute maximum of a module, use this line.
    return;
  }

  // if telemetry is not arrived, quick return.
  if (!dynamic_power_updated)
    return;
  dynamic_power_updated = false;

  // =============  LQ-based power boost up ==============
  // Quick boost up of power when detected any emergency LQ drops.
  // It should be useful for bando or sudden lost of LoS cases.
  int32_t lq_current = crsf.LinkStatistics.uplink_Link_quality;
  int32_t lq_diff = (dynamic_power_avg_lq>>16) - lq_current;
  // if LQ drops quickly (DYNAMIC_POWER_BOOST_LQ_THRESHOLD) or critically low below DYNAMIC_POWER_BOOST_LQ_MIN, immediately boost to the configured max power.
  if(lq_diff >= DYNAMIC_POWER_BOOST_LQ_THRESHOLD || lq_current <= DYNAMIC_POWER_BOOST_LQ_MIN)
  {
      POWERMGNT.setPower((PowerLevels_e)config.GetPower());
      // restart the rssi sampling after a boost up
      dynamic_power_rssi_sum = 0;
      dynamic_power_rssi_n = 0;
  }
  // Moving average calculation, multiplied by 2^16 for avoiding (costly) floating point operation, while maintaining some fraction parts.
  dynamic_power_avg_lq = ((int32_t)(DYNAMIC_POWER_MOVING_AVG_K - 1) * dynamic_power_avg_lq + (lq_current<<16)) / DYNAMIC_POWER_MOVING_AVG_K;

  // =============  RSSI-based power adjustment ==============
  // It is working slowly, suitable for a general long-range flights.

  // Get the RSSI from the selected antenna.
  int8_t rssi = (crsf.LinkStatistics.active_antenna == 0)? crsf.LinkStatistics.uplink_RSSI_1: crsf.LinkStatistics.uplink_RSSI_2;

  dynamic_power_rssi_sum += rssi;
  dynamic_power_rssi_n++;

  // Dynamic power needs at least DYNAMIC_POWER_MIN_RECORD_NUM amount of telemetry records to update.
  if(dynamic_power_rssi_n < DYNAMIC_POWER_MIN_RECORD_NUM)
    return;

  int32_t avg_rssi = dynamic_power_rssi_sum / dynamic_power_rssi_n;
  int32_t expected_RXsensitivity = ExpressLRS_currAirRate_RFperfParams->RXsensitivity;

  int32_t rssi_inc_threshold = expected_RXsensitivity + DYNPOWER_THRESH_UP;
  int32_t rssi_dec_threshold = expected_RXsensitivity + DYNPOWER_THRESH_DN;

  // increase power only up to the set power from the LUA script
  if (avg_rssi < rssi_inc_threshold && POWERMGNT.currPower() < (PowerLevels_e)config.GetPower()) {
    DBGLN("Power increase");
    POWERMGNT.incPower();
  }
  if (avg_rssi > rssi_dec_threshold) {
    DBGLN("Power decrease");
    POWERMGNT.decPower();
  }

  dynamic_power_rssi_sum = 0;
  dynamic_power_rssi_n = 0;
}

void ICACHE_RAM_ATTR ProcessTLMpacket()
{
  uint16_t inCRC = (((uint16_t)Radio.RXdataBuffer[0] & 0b11111100) << 6) | Radio.RXdataBuffer[7];

  Radio.RXdataBuffer[0] &= 0b11;
  uint16_t calculatedCRC = ota_crc.calc(Radio.RXdataBuffer, 7, CRCInitializer);

  uint8_t type = Radio.RXdataBuffer[0] & TLM_PACKET;
  uint8_t TLMheader = Radio.RXdataBuffer[1];

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

  if (connectionState != connected)
  {
    connectionState = connected;
    VtxConfigReadyToSend = true;
    DBGLN("got downlink conn");
  }

  LastTLMpacketRecvMillis = millis();
  LQCalc.add();

    switch(TLMheader & ELRS_TELEMETRY_TYPE_MASK)
    {
        case ELRS_TELEMETRY_TYPE_LINK:
            // Antenna is the high bit in the RSSI_1 value
            crsf.LinkStatistics.active_antenna = Radio.RXdataBuffer[2] >> 7;
            // RSSI received is signed, inverted polarity (positive value = -dBm)
            // OpenTX's value is signed and will display +dBm and -dBm properly
            crsf.LinkStatistics.uplink_RSSI_1 = -(Radio.RXdataBuffer[2] & 0x7f);
            crsf.LinkStatistics.uplink_RSSI_2 = -(Radio.RXdataBuffer[3]);
            crsf.LinkStatistics.uplink_SNR = Radio.RXdataBuffer[4];
            crsf.LinkStatistics.uplink_Link_quality = Radio.RXdataBuffer[5];
            crsf.LinkStatistics.downlink_SNR = Radio.LastPacketSNR;
            crsf.LinkStatistics.downlink_RSSI = Radio.LastPacketRSSI;
            // -- uplink_TX_Power is updated when sending to the handset, so it updates when missing telemetry
            // -- rf_mode is updated when we change rates
            // -- downlink_Link_quality is updated before the LQ period is incremented
            MspSender.ConfirmCurrentPayload(Radio.RXdataBuffer[6] == 1);

            dynamic_power_updated = true;
            break;

        case ELRS_TELEMETRY_TYPE_DATA:
            TelemetryReceiver.ReceiveData(TLMheader >> ELRS_TELEMETRY_SHIFT, Radio.RXdataBuffer + 2);
            break;
    }
}

void ICACHE_RAM_ATTR GenerateSyncPacketData()
{
  const uint8_t SwitchEncMode = config.GetSwitchMode() & 0b11;
  uint8_t Index;
  if (syncSpamCounter)
  {
    Index = (config.GetRate() & 0b11);
  }
  else
  {
    Index = (ExpressLRS_currAirRate_Modparams->index & 0b11);
  }

  // TLM ratio is boosted for one sync cycle when the MspSender goes active
  if (MspSender.IsActive())
    ExpressLRS_currAirRate_Modparams->TLMinterval = TLM_RATIO_1_2;
  else
    ExpressLRS_currAirRate_Modparams->TLMinterval = (expresslrs_tlm_ratio_e)config.GetTlm();
  uint8_t TLMrate = (ExpressLRS_currAirRate_Modparams->TLMinterval & 0b111);

  Radio.TXdataBuffer[0] = SYNC_PACKET & 0b11;
  Radio.TXdataBuffer[1] = FHSSgetCurrIndex();
  Radio.TXdataBuffer[2] = NonceTX;
  Radio.TXdataBuffer[3] = (Index << 6) + (TLMrate << 3) + (SwitchEncMode << 1);
  Radio.TXdataBuffer[4] = UID[3];
  Radio.TXdataBuffer[5] = UID[4];
  Radio.TXdataBuffer[6] = UID[5];
  // For model match, the last byte of the binding ID is XORed with the inverse of the modelId
  if (!InBindingMode && config.GetModelMatch())
  {
    Radio.TXdataBuffer[6] ^= (~crsf.getModelID()) & MODELMATCH_MASK;
  }

  SyncPacketLastSent = millis();
  if (syncSpamCounter)
    --syncSpamCounter;
}

uint8_t adjustPacketRateForBaud(uint8_t rate)
{
  #if defined(Regulatory_Domain_ISM_2400)
    // Packet rate limited to 250Hz if we are on 115k baud
    if (crsf.GetCurrentBaudRate() == 115200) {
      while(rate < RATE_MAX) {
        expresslrs_mod_settings_s *const ModParams = get_elrs_airRateConfig(rate);
        if (ModParams->enum_rate >= RATE_250HZ) {
          break;
        }
        rate++;
      }
    }
  #endif
  return rate;
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

  DBGLN("set rate");
  hwTimer.updateInterval(ModParams->interval);
  Radio.Config(ModParams->bw, ModParams->sf, ModParams->cr, GetInitialFreq(), ModParams->PreambleLen, invertIQ, ModParams->PayloadLength);

  ExpressLRS_currAirRate_Modparams = ModParams;
  ExpressLRS_currAirRate_RFperfParams = RFperf;
  crsf.LinkStatistics.rf_Mode = (uint8_t)RATE_4HZ - (uint8_t)ExpressLRS_currAirRate_Modparams->enum_rate;

  crsf.setSyncParams(ModParams->interval);
  connectionState = disconnected;
  rfModeLastChangedMS = millis();
}

void ICACHE_RAM_ATTR HandleFHSS()
{
  if (InBindingMode)
  {
    return;
  }

  uint8_t modresult = (NonceTX) % ExpressLRS_currAirRate_Modparams->FHSShopInterval;

  if (modresult == 0) // if it time to hop, do so.
  {
    Radio.SetFrequencyReg(FHSSgetNextFreq());
  }
}

void ICACHE_RAM_ATTR HandleTLM()
{
  if (ExpressLRS_currAirRate_Modparams->TLMinterval > 0)
  {
    uint8_t modresult = (NonceTX) % TLMratioEnumToValue(ExpressLRS_currAirRate_Modparams->TLMinterval);
    if (modresult != 0) // wait for tlm response because it's time
    {
      return;
    }
    Radio.RXnb();
    WaitRXresponse = true;
  }
}

void ICACHE_RAM_ATTR SendRCdataToRF()
{
  uint8_t *data;
  uint8_t maxLength;
  uint8_t packageIndex;
#ifdef FEATURE_OPENTX_SYNC
  crsf.JustSentRFpacket(); // tells the crsf that we want to send data now - this allows opentx packet syncing
#endif

  /////// This Part Handles the Telemetry Response ///////
  if ((uint8_t)ExpressLRS_currAirRate_Modparams->TLMinterval > 0)
  {
    uint8_t modresult = (NonceTX) % TLMratioEnumToValue(ExpressLRS_currAirRate_Modparams->TLMinterval);
    if (modresult == 0)
    { // wait for tlm response
      if (WaitRXresponse == true)
      {
        WaitRXresponse = false;
        crsf.LinkStatistics.downlink_Link_quality = LQCalc.getLQ();
        LQCalc.inc();
        return;
      }
      else
      {
        NonceTX++;
      }
    }
  }

  uint32_t now = millis();
  static uint8_t syncSlot;
#if defined(NO_SYNC_ON_ARM)
  uint32_t SyncInterval = 250;
  bool skipSync = IsArmed();
#else
  uint32_t SyncInterval = (connectionState == connected) ? ExpressLRS_currAirRate_RFperfParams->SyncPktIntervalConnected : ExpressLRS_currAirRate_RFperfParams->SyncPktIntervalDisconnected;
  bool skipSync = false;
#endif

  uint8_t NonceFHSSresult = NonceTX % ExpressLRS_currAirRate_Modparams->FHSShopInterval;
  bool WithinSyncSpamResidualWindow = now - rfModeLastChangedMS < syncSpamAResidualTimeMS;

  // Sync spam only happens on slot 1 and 2 and can't be disabled
  if ((syncSpamCounter || WithinSyncSpamResidualWindow) && (NonceFHSSresult == 1 || NonceFHSSresult == 2))
  {
    GenerateSyncPacketData();
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
  if (Radio.TXdataBuffer[0] != SYNC_PACKET && OtaSwitchModeCurrent == smHybridWide)
    Radio.TXdataBuffer[0] |= NonceFHSSresult << 2;

  ///// Next, Calculate the CRC and put it into the buffer /////
  uint16_t crc = ota_crc.calc(Radio.TXdataBuffer, 7, CRCInitializer);
  Radio.TXdataBuffer[0] = (Radio.TXdataBuffer[0] & 0b11) | ((crc >> 6) & 0b11111100);
  Radio.TXdataBuffer[7] = crc & 0xFF;

  Radio.TXnb();
}

/*
 * Called as the timer ISR when transmitting
 */
void ICACHE_RAM_ATTR timerCallbackNormal()
{
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
  if (NonceTX % ExpressLRS_currAirRate_Modparams->FHSShopInterval == 0)
  {
    FHSSptr++;
  }
}

#if defined(PLATFORM_ESP32) || defined(PLATFORM_ESP8266)
static void beginWebsever()
{
    hwTimer.stop();
    // Set transmit power to minimum
    POWERMGNT.setPower(MinPower);
    connectionState = wifiUpdate;
    BeginWebUpdate();
}
#endif

void registerLuaParameters() {
  registerLUAParameter(&luaAirRate, [](uint8_t id, uint8_t arg){
    if ((arg < RATE_MAX) && (arg >= 0))
    {
      DBGLN("Request AirRate: %d", arg);
      uint8_t rate = RATE_MAX - 1 - arg;
      rate = adjustPacketRateForBaud(rate);
      config.SetRate(rate);
      #if defined(HAS_OLED)
        OLED.updateScreen(OLED.getPowerString((PowerLevels_e)POWERMGNT.currPower()),
                          OLED.getRateString((expresslrs_RFrates_e)RATE_MAX - arg),
                          OLED.getTLMRatioString((expresslrs_tlm_ratio_e)(ExpressLRS_currAirRate_Modparams->TLMinterval)), commitStr);
      #endif
    }
  });
  registerLUAParameter(&luaTlmRate, [](uint8_t id, uint8_t arg){
    if ((arg <= (uint8_t)TLM_RATIO_1_2) && (arg >= (uint8_t)TLM_RATIO_NO_TLM))
    {
      DBGLN("Request TLM interval: %d", arg);
      config.SetTlm((expresslrs_tlm_ratio_e)arg);
      #if defined(HAS_OLED)
        OLED.updateScreen(OLED.getPowerString((PowerLevels_e)POWERMGNT.currPower()),
                          OLED.getRateString((expresslrs_RFrates_e)ExpressLRS_currAirRate_Modparams->enum_rate),
                          OLED.getTLMRatioString((expresslrs_tlm_ratio_e)arg), commitStr);
      #endif
    }
  });
  registerLUAParameter(&luaSwitch, [](uint8_t id, uint8_t arg){
    // Only allow changing switch mode when disconnected since we need to guarantee
    // the pack and unpack functions are matched
    if (connectionState == disconnected)
    {
      DBGLN("Request Switch Mode: %d", arg);
      // +1 to the mode because 1-bit was mode 0 and has been removed
      // The modes should be updated for 1.1RC so mode 0 can be smHybrid
      uint32_t newSwitchMode = (crsf.ParameterUpdateData[2] + 1) & 0b11;
      config.SetSwitchMode(newSwitchMode);
      OtaSetSwitchMode((OtaSwitchMode_e)newSwitchMode);
    }
  });
  registerLUAParameter(&luaModelMatch, [](uint8_t id, uint8_t arg){
    bool newModelMatch = crsf.ParameterUpdateData[2] & 0b1;
#ifndef DEBUG_SUPPRESS
    DBGLN("Request Model Match: %d", newModelMatch);
#endif
    config.SetModelMatch(newModelMatch);
    if (connectionState == connected) {
      mspPacket_t msp;
      msp.reset();
      msp.makeCommand();
      msp.function = MSP_SET_RX_CONFIG;
      msp.addByte(MSP_ELRS_MODEL_ID);
      msp.addByte(newModelMatch ? crsf.getModelID() : 0xff);
      crsf.AddMspMessage(&msp);
    }
  });
  registerLUAParameter(&luaPowerFolder);
  registerLUAParameter(&luaPower, [](uint8_t id, uint8_t arg){
    PowerLevels_e newPower = (PowerLevels_e)arg;
    DBGLN("Request Power: %d", newPower);
    
    if (newPower > MaxPower)
    {
        newPower = MaxPower;
    } else if (newPower < MinPower)
    {
        newPower = MinPower;
    }
    config.SetPower(newPower);

    #if defined(HAS_OLED)
      OLED.updateScreen(OLED.getPowerString((PowerLevels_e)arg),
                        OLED.getRateString((expresslrs_RFrates_e)ExpressLRS_currAirRate_Modparams->enum_rate),
                        OLED.getTLMRatioString((expresslrs_tlm_ratio_e)ExpressLRS_currAirRate_Modparams->TLMinterval), commitStr);
    #endif
  }, luaPowerFolder.luaProperties1.id);
  registerLUAParameter(&luaDynamicPower, [](uint8_t id, uint8_t arg){
      config.SetDynamicPower(arg > 0);
      config.SetBoostChannel((arg - 1) > 0 ? arg - 1 : 0);
  }, luaPowerFolder.luaProperties1.id);
  registerLUAParameter(&luaVtxFolder);
  registerLUAParameter(&luaVtxBand, [](uint8_t id, uint8_t arg){
      config.SetVtxBand(arg);
  },luaVtxFolder.luaProperties1.id);
  registerLUAParameter(&luaVtxChannel, [](uint8_t id, uint8_t arg){
      config.SetVtxChannel(arg);
  },luaVtxFolder.luaProperties1.id);
  registerLUAParameter(&luaVtxPwr, [](uint8_t id, uint8_t arg){
      config.SetVtxPower(arg);
  },luaVtxFolder.luaProperties1.id);
  registerLUAParameter(&luaVtxPit, [](uint8_t id, uint8_t arg){
      config.SetVtxPitmode(arg);
  },luaVtxFolder.luaProperties1.id);
  registerLUAParameter(&luaVtxSend, [](uint8_t id, uint8_t arg){
    if (arg < 5) {
      VtxConfigReadyToSend = true;
      sendLuaCommandResponse(&luaVtxSend, 2, "Sending...");
    } else {
      sendLuaCommandResponse(&luaVtxSend, 0, "Config Sent");
    }
  },luaVtxFolder.luaProperties1.id);

  registerLUAParameter(&luaBind, [](uint8_t id, uint8_t arg){
    if (arg < 5) {
      DBGLN("Binding requested from LUA");
      EnterBindingMode();
      sendLuaCommandResponse(&luaBind, 2, "Binding...");
    } else {
      sendLuaCommandResponse(&luaBind, InBindingMode ? 2 : 0, InBindingMode ? "Binding..." : "Binding Sent");
    }
  });
  #ifdef PLATFORM_ESP32
    registerLUAParameter(&luaWebUpdate, [](uint8_t id, uint8_t arg){
      DBGVLN("arg %d", arg);
      if (arg == 4) // 4 = request confirmed, start
      {
        //confirm run on ELRSv2.lua or start command from CRSF configurator,
        //since ELRS LUA can do 2 step confirmation, it needs confirmation to start wifi to prevent stuck on
        //unintentional button press.
        DBGLN("Wifi Update Mode Requested!");
        sendLuaCommandResponse(&luaWebUpdate, 2, "Wifi Running...");
        beginWebsever();
      }
      else if (arg > 0 && arg < 4)
      {
        sendLuaCommandResponse(&luaWebUpdate, 3, "Enter WiFi Update Mode?");
      }
      else if (arg == 5)
      {
        sendLuaCommandResponse(&luaWebUpdate, 0, "WiFi Cancelled");
        if (connectionState == wifiUpdate) {
          rebootTime = millis() + 400;
        }
      }
      else
      {
        sendLuaCommandResponse(&luaWebUpdate, luaWebUpdate.luaProperties2.status, luaWebUpdate.label2);
      }
    });

    registerLUAParameter(&luaBLEJoystick, [](uint8_t id, uint8_t arg){
      if (arg == 4) // 4 = request confirmed, start
      {
        //confirm run on ELRSv2.lua or start command from CRSF configurator,
        //since ELRS LUA can do 2 step confirmation, it needs confirmation to start wifi to prevent stuck on
        //unintentional button press.
        connectionState = bleJoystick;
        DBGLN("BLE Joystick Mode Requested!");
        hwTimer.stop();
        crsf.RCdataCallback = &BluetoothJoystickUpdateValues;
        hwTimer.updateInterval(5000);
        crsf.setSyncParams(5000); // 200hz
        delay(100);
        crsf.disableOpentxSync();
        POWERMGNT.setPower(MinPower);
        Radio.End();
        BluetoothJoystickBegin();
        sendLuaCommandResponse(&luaBLEJoystick, 2, "Joystick Running...");
      }
      else if (arg > 0 && arg < 4) //start command, 1 = start, 2 = running, 3 = request confirmation
      {
        sendLuaCommandResponse(&luaBLEJoystick, 3, "Start BLE Joystick?");
      }
      else if (arg == 5)
      {
        sendLuaCommandResponse(&luaBLEJoystick, 0, "Joystick Cancelled");
        if (connectionState == bleJoystick) {
          rebootTime = millis() + 400;
        }
      }
      else
      {
        sendLuaCommandResponse(&luaBLEJoystick, luaBLEJoystick.luaProperties2.status, luaBLEJoystick.label2);
      }
    });

  #endif

  registerLUAParameter(&luaInfo);
  registerLUAParameter(&luaELRSversion);
  registerLUAParameter(NULL);
}

void resetLuaParams(){
  uint8_t rate = adjustPacketRateForBaud(config.GetRate());
  setLuaTextSelectionValue(&luaAirRate, RATE_MAX - 1 - rate);
  setLuaTextSelectionValue(&luaTlmRate, config.GetTlm());
  setLuaTextSelectionValue(&luaSwitch,(uint8_t)(config.GetSwitchMode() - 1)); // -1 for missing sm1Bit
  setLuaTextSelectionValue(&luaModelMatch,(uint8_t)config.GetModelMatch());

  setLuaTextSelectionValue(&luaPower, config.GetPower());

  uint8_t dynamic = config.GetDynamicPower() ? config.GetBoostChannel() + 1 : 0;
  setLuaTextSelectionValue(&luaDynamicPower,dynamic);

  setLuaTextSelectionValue(&luaVtxBand,config.GetVtxBand());
  setLuaTextSelectionValue(&luaVtxChannel,config.GetVtxChannel());
  setLuaTextSelectionValue(&luaVtxPwr,config.GetVtxPower());
  setLuaTextSelectionValue(&luaVtxPit,config.GetVtxPitmode());
}

void updateLUApacketCount(){
  itoa(crsf.BadPktsCountResult, luaBadGoodString, 10);
  strcat(luaBadGoodString, "/");
  itoa(crsf.GoodPktsCountResult, luaBadGoodString + strlen(luaBadGoodString), 10);
  setLuaStringValue(&luaInfo, luaBadGoodString);
  resetLuaParams();
}

void UARTdisconnected()
{
  #if defined(GPIO_PIN_BUZZER)
  const uint16_t beepFreq[] = {676, 520};
  const uint16_t beepDurations[] = {300, 150};
  for (int i = 0; i < 2; i++)
  {
    tone(GPIO_PIN_BUZZER, beepFreq[i], beepDurations[i]);
    delay(beepDurations[i]);
    noTone(GPIO_PIN_BUZZER);
  }
  pinMode(GPIO_PIN_BUZZER, INPUT);
  #endif
  hwTimer.stop();
  connectionState = noCrossfire;
}

void UARTconnected()
{
  #if defined(GPIO_PIN_BUZZER) && !defined(DISABLE_STARTUP_BEEP)
  const uint16_t beepFreq[] = {520, 676};
  const uint16_t beepDurations[] = {150, 300};
  for (int i = 0; i < 2; i++)
  {
    tone(GPIO_PIN_BUZZER, beepFreq[i], beepDurations[i]);
    delay(beepDurations[i]);
    noTone(GPIO_PIN_BUZZER);
  }
  pinMode(GPIO_PIN_BUZZER, INPUT);
  #endif

  rfModeLastChangedMS = millis(); // force syncspam on first packets
  hwTimer.resume();
  crsfSeen = true;
  SetRFLinkRate(config.GetRate());
}

static void ChangeRadioParams()
{
  ModelUpdatePending = false;

  SetRFLinkRate(config.GetRate());
  POWERMGNT.setPower((PowerLevels_e)config.GetPower());
  OtaSetSwitchMode((OtaSwitchMode_e)config.GetSwitchMode());
  // TLM interval is set on the next SYNC packet
}

void HandleUpdateParameter()
{
  bool updated = luaHandleUpdateParameter();
  if (updated && config.IsModified())
  {
    syncSpamCounter = syncSpamAmount;
  }
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
  ChangeRadioParams();

  // Write the uncommitted eeprom values
  config.Commit();
  hwTimer.callbackTock = &timerCallbackNormal; // Resume the timer
  resetLuaParams();
}


static void CheckConfigChangePending()
{
  if (config.IsModified() || ModelUpdatePending)
  {
    // Keep transmitting sync packets until the spam counter runs out
    if (syncSpamCounter > 0)
      return;

#if !defined(PLATFORM_STM32) && !defined(TARGET_USE_EEPROM)
    while (busyTransmitting); // wait until no longer transmitting
    hwTimer.callbackTock = &timerCallbackIdle;
#else
    // The code expects to enter here shortly after the tock ISR has started sending the last
    // sync packet, before the tick ISR. Because the EEPROM write takes so long and disables
    // interrupts, FastForward the timer
    const uint32_t EEPROM_WRITE_DURATION = 30000; // us, ~27ms is where it starts getting off by one
    const uint32_t cycleInterval = get_elrs_airRateConfig(config.GetRate())->interval;
    // Total time needs to be at least DURATION, rounded up to next cycle
    uint32_t pauseCycles = (EEPROM_WRITE_DURATION + cycleInterval - 1) / cycleInterval;
    // Pause won't return until paused, and has just passed the tick ISR (but not fired)
    hwTimer.pause(pauseCycles * cycleInterval);

    while (busyTransmitting); // wait until no longer transmitting

    --pauseCycles; // the last cycle will actually be a transmit
    while (pauseCycles--)
      timerCallbackIdle();
#endif
    ConfigChangeCommit();
  }
}

void ICACHE_RAM_ATTR RXdoneISR()
{
  ProcessTLMpacket();
  busyTransmitting = false;
}

void ICACHE_RAM_ATTR TXdoneISR()
{
  busyTransmitting = false;
  NonceTX++; // must be done before callback
  HandleFHSS();
  HandleTLM();
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
    connectionState = connected;
    #if defined(GPIO_PIN_LED_RED) && (GPIO_PIN_LED_RED != UNDEF_PIN)
    digitalWrite(GPIO_PIN_LED_RED, HIGH ^ GPIO_LED_RED_INVERTED);
    #endif // GPIO_PIN_LED_RED
  }
  else
  {
    connectionState = disconnected;
    #if defined(GPIO_PIN_LED_RED) && (GPIO_PIN_LED_RED != UNDEF_PIN)
    digitalWrite(GPIO_PIN_LED_RED, LOW ^ GPIO_LED_RED_INVERTED);
    #endif // GPIO_PIN_LED_RED
  }
}

void setup()
{
#if defined(TARGET_TX_GHOST)
  Serial.setTx(PA2);
  Serial.setRx(PA3);
#endif
  Serial.begin(460800);
#if defined(HAS_OLED)
  OLED.displayLogo();
//  OLED.setCommitString(thisCommit, commitStr);
#endif

  startupLEDs();

  #if defined(GPIO_PIN_LED_GREEN) && (GPIO_PIN_LED_GREEN != UNDEF_PIN)
    pinMode(GPIO_PIN_LED_GREEN, OUTPUT);
    digitalWrite(GPIO_PIN_LED_GREEN, HIGH ^ GPIO_LED_GREEN_INVERTED);
  #endif // GPIO_PIN_LED_GREEN
  #if defined(GPIO_PIN_LED_RED) && (GPIO_PIN_LED_RED != UNDEF_PIN)
    pinMode(GPIO_PIN_LED_RED, OUTPUT);
    digitalWrite(GPIO_PIN_LED_RED, LOW ^ GPIO_LED_RED_INVERTED);
  #endif // GPIO_PIN_LED_RED

  #if defined(GPIO_PIN_BUZZER) && (GPIO_PIN_BUZZER != UNDEF_PIN) && !defined(DISABLE_STARTUP_BEEP)
    pinMode(GPIO_PIN_BUZZER, OUTPUT);
    // Annoying startup beeps
    #ifndef JUST_BEEP_ONCE
      #if defined(MY_STARTUP_MELODY_ARR)
        // It's silly but I couldn't help myself. See: BLHeli32 startup tones.
        const int melody[][2] = MY_STARTUP_MELODY_ARR;

        for(uint i = 0; i < sizeof(melody) / sizeof(melody[0]); i++) {
          tone(GPIO_PIN_BUZZER, melody[i][0], melody[i][1]);
          delay(melody[i][1]);
          noTone(GPIO_PIN_BUZZER);
        }
      #else
        // use default jingle
        const int beepFreq[] = {659, 659, 523, 659, 783, 392};
        const int beepDurations[] = {300, 300, 100, 300, 550, 575};

        for (int i = 0; i < 6; i++)
        {
          tone(GPIO_PIN_BUZZER, beepFreq[i], beepDurations[i]);
          delay(beepDurations[i]);
          noTone(GPIO_PIN_BUZZER);
        }
      #endif
    #else
      tone(GPIO_PIN_BUZZER, 400, 200);
      delay(200);
      tone(GPIO_PIN_BUZZER, 480, 200);
    #endif
  #endif // GPIO_PIN_BUZZER

#if defined(TARGET_TX_FM30)
  pinMode(GPIO_PIN_LED_RED_GREEN, OUTPUT); // Green LED on "Red" LED (off)
  digitalWrite(GPIO_PIN_LED_RED_GREEN, HIGH);
  pinMode(GPIO_PIN_LED_GREEN_RED, OUTPUT); // Red LED on "Green" LED (off)
  digitalWrite(GPIO_PIN_LED_GREEN_RED, HIGH);
  pinMode(GPIO_PIN_UART3RX_INVERT, OUTPUT); // RX3 inverter (from radio)
  digitalWrite(GPIO_PIN_UART3RX_INVERT, LOW); // RX3 not inverted
  pinMode(GPIO_PIN_BLUETOOTH_EN, OUTPUT); // Bluetooth enable (disabled)
  digitalWrite(GPIO_PIN_BLUETOOTH_EN, HIGH);
  pinMode(GPIO_PIN_UART1RX_INVERT, OUTPUT); // RX1 inverter (TX handled in CRSF)
  digitalWrite(GPIO_PIN_UART1RX_INVERT, HIGH);
#endif
#if defined(TARGET_TX_FM30_MINI)
    pinMode(GPIO_PIN_UART1TX_INVERT, OUTPUT); // TX1 inverter used for debug
    digitalWrite(GPIO_PIN_UART1TX_INVERT, LOW);
#endif

#if defined(TARGET_TX_BETAFPV_2400_V1) || defined(TARGET_TX_BETAFPV_900_V1)
  button.OnShortPress = []() { if (button.getCount() == 3) EnterBindingMode(); };
  button.OnLongPress = &POWERMGNT.handleCyclePower;
#endif

#ifdef PLATFORM_ESP32
  // Get base mac address
  esp_read_mac(baseMac, ESP_MAC_WIFI_STA);
  // UID[0..2] are OUI (organisationally unique identifier) and are not ESP32 unique.  Do not use!
#endif // PLATFORM_ESP32

  FHSSrandomiseFHSSsequence(uidMacSeedGet());

  Radio.RXdoneCallback = &RXdoneISR;
  Radio.TXdoneCallback = &TXdoneISR;

  crsf.connected = &UARTconnected; // it will auto init when it detects UART connection
  crsf.disconnected = &UARTdisconnected;
  crsf.RecvParameterUpdate = &luaParamUpdateReq;
  crsf.RecvModelUpdate = &ModelUpdateReq;
  hwTimer.callbackTock = &timerCallbackNormal;
  DBGLN("ExpressLRS TX Module Booted...");

  eeprom.Begin(); // Init the eeprom
  config.SetStorageProvider(&eeprom); // Pass pointer to the Config class for access to storage
  config.Load(); // Load the stored values from eeprom

  Radio.currFreq = GetInitialFreq(); //set frequency first or an error will occur!!!
  #if !defined(Regulatory_Domain_ISM_2400)
  //Radio.currSyncWord = UID[3];
  #endif
  bool init_success = Radio.Begin();
  if (!init_success)
  {
    #ifdef PLATFORM_ESP32
    connectionState = radioFailed;
    BeginWebUpdate();
    while (1)
    {
      updateLEDs(millis(), radioFailed, 0, 0);
      HandleWebUpdate();
      delay(1);
    }
    #endif
    #if defined(GPIO_PIN_LED_GREEN) && (GPIO_PIN_LED_GREEN != UNDEF_PIN)
      digitalWrite(GPIO_PIN_LED_GREEN, LOW ^ GPIO_LED_GREEN_INVERTED);
    #endif // GPIO_PIN_LED_GREEN
    #if defined(GPIO_PIN_BUZZER) && (GPIO_PIN_BUZZER != UNDEF_PIN)
      tone(GPIO_PIN_BUZZER, 480, 200);
    #endif // GPIO_PIN_BUZZER
    #if defined(GPIO_PIN_LED_RED) && (GPIO_PIN_LED_RED != UNDEF_PIN)
      digitalWrite(GPIO_PIN_LED_RED, LOW ^ GPIO_LED_RED_INVERTED);
    #endif // GPIO_PIN_LED_RED
    delay(200);
    #if defined(GPIO_PIN_BUZZER) && (GPIO_PIN_BUZZER != UNDEF_PIN)
      tone(GPIO_PIN_BUZZER, 400, 200);
    #endif // GPIO_PIN_BUZZER
    #if defined(GPIO_PIN_LED_RED) && (GPIO_PIN_LED_RED != UNDEF_PIN)
      digitalWrite(GPIO_PIN_LED_RED, HIGH ^ GPIO_LED_RED_INVERTED);
    #endif // GPIO_PIN_LED_RED
    delay(1000);
  }
  TelemetryReceiver.SetDataToReceive(sizeof(CRSFinBuffer), CRSFinBuffer, ELRS_TELEMETRY_BYTES_PER_CALL);

  POWERMGNT.init();

  // Set the pkt rate, TLM ratio, and power from the stored eeprom values
  ChangeRadioParams();

  registerLuaParameters();
  registerLUAPopulateParams(updateLUApacketCount);

  hwTimer.init();
  //hwTimer.resume();  //uncomment to automatically start the RX timer and leave it running
  connectionState = noCrossfire;
  crsf.Begin();
  #if defined(HAS_OLED)
    OLED.updateScreen(OLED.getPowerString((PowerLevels_e)POWERMGNT.currPower()),
                  OLED.getRateString((expresslrs_RFrates_e)ExpressLRS_currAirRate_Modparams->enum_rate),
                  OLED.getTLMRatioString((expresslrs_tlm_ratio_e)(ExpressLRS_currAirRate_Modparams->TLMinterval)), commitStr);
  #endif
}

void loop()
{
  uint32_t now = millis();
  static bool mspTransferActive = false;

  if (connectionState < MODE_STATES)
  {
    UpdateConnectDisconnectStatus();
  }

  updateLEDs(now, connectionState, ExpressLRS_currAirRate_Modparams->index, POWERMGNT.currPower());

  HandleUpdateParameter();
  CheckConfigChangePending();

#if defined(PLATFORM_ESP32)
  // If the reboot time is set and the current time is past the reboot time then reboot.
  if (rebootTime != 0 && now > rebootTime) {
    ESP.restart();
  }

  #if defined(AUTO_WIFI_ON_INTERVAL)
    //if webupdate was requested before or AUTO_WIFI_ON_INTERVAL has been elapsed but uart is not detected
    //start webupdate, there might be wrong configuration flashed.
    if(crsfSeen == false && now > (AUTO_WIFI_ON_INTERVAL * 1000) && connectionState < MODE_STATES){
      DBGLN("No CRSF ever detected, starting WiFi");
      beginWebsever();
    }
  #endif
  if (connectionState == wifiUpdate)
  {
    HandleWebUpdate();
    return;
  }
#endif

  #ifdef FEATURE_OPENTX_SYNC
    // DBGVLN(crsf.OpenTXsyncOffset);
  #endif

  #ifdef PLATFORM_STM32
    crsf.handleUARTin();
  #endif // PLATFORM_STM32

  #if defined(GPIO_PIN_BUTTON) && (GPIO_PIN_BUTTON != UNDEF_PIN)
    button.update();
  #endif

  if (Serial.available())
  {
    uint8_t c = Serial.read();

    if (msp.processReceivedByte(c))
    {
      // Finished processing a complete packet
      ProcessMSPPacket(msp.getReceivedPacket());
      msp.markPacketReceived();
    }
  }

  if (VtxConfigReadyToSend)
  {
    VtxConfigReadyToSend = false;
    VtxConfigToMSPOut();
  }

  /* Send TLM updates to handset if connected + reporting period
   * is elapsed. This keeps handset happy dispite of the telemetry ratio */
  if ((connectionState == connected) && (LastTLMpacketRecvMillis != 0) &&
      (now >= (uint32_t)(TLM_REPORT_INTERVAL_MS + TLMpacketReported))) {
    crsf.LinkStatistics.uplink_TX_Power = POWERMGNT.powerToCrsfPower(POWERMGNT.currPower());
    crsf.sendLinkStatisticsToTX();
    TLMpacketReported = now;
  }


  if (TelemetryReceiver.HasFinishedData())
  {
      crsf.sendTelemetryToTX(CRSFinBuffer);
      TelemetryReceiver.Unlock();
  }

  // Actual update of dynamic power is done here
  DynamicPower_Update();

  // only send msp data when binding is not active
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
      uint8_t* currentMspData = crsf.GetMspMessage();
      // if we have a new msp package start sending
      if (currentMspData != NULL)
      {
        MspSender.SetDataToTransmit(ELRS_MSP_BUFFER, currentMspData, ELRS_MSP_BYTES_PER_CALL);
        mspTransferActive = true;
      }
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
  case RATE_200HZ:
  case RATE_100HZ:
  case RATE_50HZ:
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

    VtxConfigReadyToSend = true;

    resetLuaParams();
    sendLuaDevicePacket();
  }
}

void VtxConfigToMSPOut()
{
  // 0 = off in the lua Band field
  // Do not send while armed
  if (!config.GetVtxBand() || IsArmed())
    return;

  uint8_t vtxIdx = (config.GetVtxBand()-1) * 8 + config.GetVtxChannel();

  mspPacket_t packet;
  packet.reset();
  packet.function = MSP_SET_VTX_CONFIG;
  packet.addByte(vtxIdx);
  packet.addByte(0);
  packet.addByte(config.GetVtxPower());
  packet.addByte(config.GetVtxPitmode());

  crsf.AddMspMessage(&packet);

  eepromWriteToMSPOut();
}

void eepromWriteToMSPOut()
{
  mspPacket_t packet;
  packet.reset();
  packet.function = MSP_EEPROM_WRITE;
  packet.addByte(0);
  packet.addByte(0);
  packet.addByte(0);
  packet.addByte(0);

  crsf.AddMspMessage(&packet);
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
  UID[0] = BindingUID[0];
  UID[1] = BindingUID[1];
  UID[2] = BindingUID[2];
  UID[3] = BindingUID[3];
  UID[4] = BindingUID[4];
  UID[5] = BindingUID[5];

  CRCInitializer = 0;

  InBindingMode = true;

  // Start attempting to bind
  // Lock the RF rate and freq while binding
  SetRFLinkRate(RATE_DEFAULT);
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
  UID[0] = MasterUID[0];
  UID[1] = MasterUID[1];
  UID[2] = MasterUID[2];
  UID[3] = MasterUID[3];
  UID[4] = MasterUID[4];
  UID[5] = MasterUID[5];

  CRCInitializer = (UID[4] << 8) | UID[5];

  InBindingMode = false;

  MspSender.ResetState();
  SetRFLinkRate(config.GetRate()); //return to original rate

  DBGLN("Exiting binding mode");
}

void SendUIDOverMSP()
{
  BindingPackage[0] = MSP_ELRS_BIND;
  BindingPackage[1] = MasterUID[2];
  BindingPackage[2] = MasterUID[3];
  BindingPackage[3] = MasterUID[4];
  BindingPackage[4] = MasterUID[5];
  MspSender.ResetState();
  BindingSendCount = 0;
  MspSender.SetDataToTransmit(5, BindingPackage, ELRS_MSP_BYTES_PER_CALL);
  InBindingMode = true;
}



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
