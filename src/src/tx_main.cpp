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

#include "luaParams.h"
#include "FHSS.h"
// #include "debug.h"
#include "POWERMGNT.h"
#include "LED.h"
#include "msp.h"
#include "msptypes.h"
#include <OTA.h>
#include "elrs_eeprom.h"
#include "config.h"
#include "hwTimer.h"
#include "LQCALC.h"
#include "LowPassFilter.h"
#include "telemetry_protocol.h"
#ifdef ENABLE_TELEMETRY
#include "stubborn_receiver.h"
#endif
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

#if defined(GPIO_PIN_BUTTON) && (GPIO_PIN_BUTTON != UNDEF_PIN)
#include "button.h"
button button;
#endif

#if (GPIO_PIN_LED_WS2812 != UNDEF_PIN) && (GPIO_PIN_LED_WS2812_FAST != UNDEF_PIN)
uint8_t LEDfadeDiv;
uint8_t LEDfade;
bool LEDfadeDir;
constexpr uint32_t LEDupdateInterval = 100;
uint32_t LEDupdateCounterMillis;
#include "STM32F3_WS2812B_LED.h"
#endif

#define DEBUG_SUPPRESS

//// CONSTANTS ////
#define RX_CONNECTION_LOST_TIMEOUT 3000LU // After 3000ms of no TLM response consider that slave has lost connection
#define MSP_PACKET_SEND_INTERVAL 10LU

#ifndef TLM_REPORT_INTERVAL_MS
#define TLM_REPORT_INTERVAL_MS 320LU // Default to 320ms
#endif

//#define LUA_PKTCOUNT_INTERVAL_MS 3000LU

volatile uint8_t allLUAparamSent = 0;  

/// define some libs to use ///
hwTimer hwTimer;
GENERIC_CRC14 ota_crc(ELRS_CRC14_POLY);
CRSF crsf;
POWERMGNT POWERMGNT;
MSP msp;
ELRS_EEPROM eeprom;
TxConfig config;
#if defined(HAS_OLED)
OLED OLED;
char commitStr[7] = "commit";
#endif

volatile uint8_t NonceTX;

bool webUpdateMode = false;

//// MSP Data Handling ///////
bool NextPacketIsMspData = false;  // if true the next packet will contain the msp data

////////////SYNC PACKET/////////
/// sync packet spamming on mode change vars ///
#define syncSpamAResidualTimeMS 1500 // we spam some more after rate change to help link get up to speed
#define syncSpamAmount 3
volatile uint8_t syncSpamCounter = 0;
uint32_t rfModeLastChangedMS = 0;
////////////////////////////////////////////////

uint32_t SyncPacketLastSent = 0;

volatile uint32_t LastTLMpacketRecvMillis = 0;
uint32_t TLMpacketReported = 0;
uint32_t LUApacketCountReported = 0;

LQCALC<10> LQCalc;
LPF LPD_DownlinkLQ(1);

volatile bool busyTransmitting;
volatile bool UpdateParamReq = false;
uint32_t HWtimerPauseDuration = 0;
//LUA VARIABLES//
#define OPENTX_LUA_UPDATE_INTERVAL 1000
uint8_t luaWarningFLags = 0;
uint8_t suppressedLuaWarningFlags = 0xFF;
uint32_t LuaLastUpdated = 0;

bool WaitRXresponse = false;
bool WaitEepromCommit = false;

uint8_t InBindingMode = 0;
uint8_t BindingPackage[5];
uint8_t BindingSendCount = 0;
void EnterBindingMode();
void ExitBindingMode();
void SendUIDOverMSP();

#ifdef ENABLE_TELEMETRY
StubbornReceiver TelemetryReceiver(ELRS_TELEMETRY_MAX_PACKAGES);
#endif
StubbornSender MspSender(ELRS_MSP_MAX_PACKAGES);
uint8_t CRSFinBuffer[CRSF_MAX_PACKET_LEN+1];
// MSP packet handling function defs
void ProcessMSPPacket(mspPacket_t *packet);
void OnRFModePacket(mspPacket_t *packet);
void OnTxPowerPacket(mspPacket_t *packet);
void OnTLMRatePacket(mspPacket_t *packet);

uint8_t baseMac[6];

#ifdef USE_DYNAMIC_POWER
#define DYNAMIC_POWER_MIN_RECORD_NUM       5 // average at least this number of records
#define DYNAMIC_POWER_BOOST_LQ_THRESHOLD  20 // If LQ is dropped suddenly for this amount (relative), immediately boost to the max power configured.
#define DYNAMIC_POWER_BOOST_LQ_MIN        50 // If LQ is below this value (absolute), immediately boost to the max power configured.
#define DYNAMIC_POWER_MOVING_AVG_K 8 // Number of previous values for calculating moving average. Best with power of 2.
static int32_t dynamic_power_rssi_sum;
static int32_t dynamic_power_rssi_n;
static int32_t dynamic_power_avg_lq;
static bool dynamic_power_updated;
#endif

// Assume this function is called inside loop(). Heavy functions goes here.
void DynamicPower_Update()
{
  #ifdef USE_DYNAMIC_POWER
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

  int32_t rssi_inc_threshold = expected_RXsensitivity + 15;
  int32_t rssi_dec_threshold = expected_RXsensitivity + 30;

  // Serial.print("Dynamic power: ");
  // Serial.print(avg_rssi); 
  // Serial.print(", "); 
  // Serial.println(dynamic_power_rssi_n);

  // Serial.print("CurrentPower: ");
  // Serial.println(POWERMGNT.currPower());

  // Serial.print("SetPower: ");
  // Serial.println((PowerLevels_e)config.GetPower());

  // increase power only up to the set power from the LUA script
  if (avg_rssi < rssi_inc_threshold && POWERMGNT.currPower() < (PowerLevels_e)config.GetPower()) {
    // Serial.print("Power increase");
    POWERMGNT.incPower();
  }
  if (avg_rssi > rssi_dec_threshold) {
    // Serial.print("Power decrease");
    POWERMGNT.decPower();
  }

  dynamic_power_rssi_sum = 0;
  dynamic_power_rssi_n = 0;

  // Serial.print(crsf.LinkStatistics.uplink_Link_quality);
  // Serial.print("/");
  // Serial.print(dynamic_power_avg_lq>>16);
  // Serial.print("/");
  // Serial.println(lq_diff);
  #endif    
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
#ifndef DEBUG_SUPPRESS
    Serial.println("TLM crc error");
#endif
    return;
  }

  if (type != TLM_PACKET)
  {
#ifndef DEBUG_SUPPRESS
    Serial.println("TLM type error");
    Serial.println(type);
#endif
    return;
  }

  if (connectionState != connected)
  {
    connectionState = connected;
    LPD_DownlinkLQ.init(100);
#ifndef DEBUG_SUPPRESS
    Serial.println("got downlink conn");
#endif
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
            crsf.LinkStatistics.uplink_TX_Power = POWERMGNT.powerToCrsfPower(POWERMGNT.currPower());
            crsf.LinkStatistics.downlink_SNR = Radio.LastPacketSNR;
            crsf.LinkStatistics.downlink_RSSI = Radio.LastPacketRSSI;
            crsf.LinkStatistics.downlink_Link_quality = LPD_DownlinkLQ.update(LQCalc.getLQ()) + 1; // +1 fixes rounding issues with filter and makes it consistent with RX LQ Calculation
            crsf.LinkStatistics.rf_Mode = (uint8_t)RATE_4HZ - (uint8_t)ExpressLRS_currAirRate_Modparams->enum_rate;
            MspSender.ConfirmCurrentPayload(Radio.RXdataBuffer[6] == 1);

            #ifdef USE_DYNAMIC_POWER
            dynamic_power_updated = true;
            #endif
            break;

        #ifdef ENABLE_TELEMETRY
        case ELRS_TELEMETRY_TYPE_DATA:
            TelemetryReceiver.ReceiveData(TLMheader >> ELRS_TELEMETRY_SHIFT, Radio.RXdataBuffer + 2);
            break;
        #endif
    }
}

void ICACHE_RAM_ATTR GenerateSyncPacketData()
{
#ifdef HYBRID_SWITCHES_8
  const uint8_t SwitchEncMode = 0b01;
#else
  const uint8_t SwitchEncMode = 0b00;
#endif
  uint8_t Index;
  uint8_t TLMrate;
  if (syncSpamCounter)
  {
    Index = (config.GetRate() & 0b11);
    TLMrate = (config.GetTlm() & 0b111);
  }
  else
  {
    Index = (ExpressLRS_currAirRate_Modparams->index & 0b11);
    TLMrate = (ExpressLRS_currAirRate_Modparams->TLMinterval & 0b111);
  }

  Radio.TXdataBuffer[0] = SYNC_PACKET & 0b11;
  Radio.TXdataBuffer[1] = FHSSgetCurrIndex();
  Radio.TXdataBuffer[2] = NonceTX;
  Radio.TXdataBuffer[3] = (Index << 6) + (TLMrate << 3) + (SwitchEncMode << 1);
  Radio.TXdataBuffer[4] = UID[3];
  Radio.TXdataBuffer[5] = UID[4];
  Radio.TXdataBuffer[6] = UID[5];

  SyncPacketLastSent = millis();
  if (syncSpamCounter)
    --syncSpamCounter;
}

void ICACHE_RAM_ATTR SetRFLinkRate(uint8_t index) // Set speed of RF link (hz)
{
  expresslrs_mod_settings_s *const ModParams = get_elrs_airRateConfig(index);
  expresslrs_rf_pref_params_s *const RFperf = get_elrs_RFperfParams(index);
  bool invertIQ = UID[5] & 0x01;
  if ((ModParams == ExpressLRS_currAirRate_Modparams)
    && (RFperf == ExpressLRS_currAirRate_RFperfParams)
    && (invertIQ == Radio.IQinverted))
    return;
#ifndef DEBUG_SUPPRESS
  Serial.println("set rate");
#endif
  hwTimer.updateInterval(ModParams->interval);
  Radio.Config(ModParams->bw, ModParams->sf, ModParams->cr, GetInitialFreq(), ModParams->PreambleLen, invertIQ);

  ExpressLRS_currAirRate_Modparams = ModParams;
  ExpressLRS_currAirRate_RFperfParams = RFperf;

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
        LQCalc.inc();
        return;
      }
      else
      {
        NonceTX++;
      }
    }
  }

  uint32_t SyncInterval;

#if defined(NO_SYNC_ON_ARM)
  SyncInterval = 250;
  bool skipSync = (bool)CRSF_to_BIT(crsf.ChannelDataIn[AUX1]);
#else
  SyncInterval = (connectionState == connected) ? ExpressLRS_currAirRate_RFperfParams->SyncPktIntervalConnected : ExpressLRS_currAirRate_RFperfParams->SyncPktIntervalDisconnected;
  bool skipSync = false;
#endif

  uint8_t NonceFHSSresult = NonceTX % ExpressLRS_currAirRate_Modparams->FHSShopInterval;
  bool NonceFHSSresultWindow = (NonceFHSSresult == 1 || NonceFHSSresult == 2) ? true : false; // restrict to the middle nonce ticks (not before or after freq chance)
  bool WithinSyncSpamResidualWindow = (millis() - rfModeLastChangedMS < syncSpamAResidualTimeMS) ? true : false;

  if ((syncSpamCounter || WithinSyncSpamResidualWindow) && NonceFHSSresultWindow)
  {
    GenerateSyncPacketData();
  }
  else if ((!skipSync) && ((millis() > (SyncPacketLastSent + SyncInterval)) && (Radio.currFreq == GetInitialFreq()) && NonceFHSSresultWindow)) // don't sync just after we changed freqs (helps with hwTimer.init() being in sync from the get go)
  {
    GenerateSyncPacketData();
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
    }
    else
    {
      // always enable msp after a channel package since the slot is only used if MspSender has data to send
      NextPacketIsMspData = true;
      #ifdef ENABLE_TELEMETRY
      GenerateChannelData(Radio.TXdataBuffer, &crsf, TelemetryReceiver.GetCurrentConfirm());
      #else
      GenerateChannelData(Radio.TXdataBuffer, &crsf);
      #endif
    }
  }

  ///// Next, Calculate the CRC and put it into the buffer /////
  uint16_t crc = ota_crc.calc(Radio.TXdataBuffer, 7, CRCInitializer);
  Radio.TXdataBuffer[0] |= (crc >> 6) & 0b11111100;
  Radio.TXdataBuffer[7] = crc & 0xFF;

  Radio.TXnb(Radio.TXdataBuffer, 8);
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

void suppressCurrentLuaWarning(void){ //0 to suppress
  suppressedLuaWarningFlags = ~luaWarningFLags;
}
uint8_t getLuaWarning(void){ //1 if alarm
return luaWarningFLags & suppressedLuaWarningFlags;
}

void sendLuaParams()
{
  uint8_t luaParams[] = {(uint8_t)crsf.BadPktsCountResult,
                         (uint8_t)((crsf.GoodPktsCountResult & 0xFF00) >> 8),
                         (uint8_t)(crsf.GoodPktsCountResult & 0xFF),
                         (uint8_t)(getLuaWarning())};

  crsf.sendELRSparam(luaParams,4, 0x2E,F(" "),4); //*elrsinfo is the info that we want to pass when there is getluawarning()
}

void resetLuaParams(){
  setLuaTextSelectionValue(&luaAirRate,(uint8_t)(ExpressLRS_currAirRate_Modparams->enum_rate));
  setLuaTextSelectionValue(&luaTlmRate,(uint8_t)(ExpressLRS_currAirRate_Modparams->TLMinterval));
  setLuaTextSelectionValue(&luaPower,(uint8_t)(POWERMGNT.currPower()));
  allLUAparamSent = 0;
}


void updateLUApacketCount(){
  setLuaUint8Value(&luaBadPkt,(uint8_t)crsf.BadPktsCountResult);
  setLuaUint16Value(&luaGoodPkt,(uint16_t)crsf.GoodPktsCountResult);
  resetLuaParams();
}

void sendLuaFieldCrsf(uint8_t idx, uint8_t chunk){

  uint8_t sentChunk = 0;
  if(!allLUAparamSent){
    switch(idx){
      case 2:
      {
        sentChunk = crsf.sendCRSFparam(CRSF_FRAMETYPE_PARAMETER_SETTINGS_ENTRY,chunk,CRSF_TEXT_SELECTION,&luaTlmRate,luaTlmRate.size);
        break;
      }
      case 3:
      {
        sentChunk = crsf.sendCRSFparam(CRSF_FRAMETYPE_PARAMETER_SETTINGS_ENTRY,chunk,CRSF_TEXT_SELECTION,&luaPower,luaPower.size);
        break;
      }
      case 4:
      {
        sentChunk = crsf.sendCRSFparam(CRSF_FRAMETYPE_PARAMETER_SETTINGS_ENTRY,chunk,CRSF_COMMAND,&luaBind,luaBind.size);
        break;
      }
      case 5:
      {
        sentChunk = crsf.sendCRSFparam(CRSF_FRAMETYPE_PARAMETER_SETTINGS_ENTRY,chunk,CRSF_COMMAND,&luaWebUpdate,luaWebUpdate.size);
        /**if(sentChunk == 0){
          allLUAparamSent = 1;
          }*/
        break;
      }
      case 6: //commit
      { 
        sentChunk = crsf.sendCRSFparam(CRSF_FRAMETYPE_PARAMETER_SETTINGS_ENTRY,chunk,CRSF_UINT8,&luaBadPkt,luaBadPkt.size);
        break;
      }
      case 7:
      { 
        sentChunk = crsf.sendCRSFparam(CRSF_FRAMETYPE_PARAMETER_SETTINGS_ENTRY,chunk,CRSF_UINT16,&luaGoodPkt,luaGoodPkt.size);
        break;
      }
      case 8:
      { 
        sentChunk = crsf.sendCRSFparam(CRSF_FRAMETYPE_PARAMETER_SETTINGS_ENTRY,chunk,CRSF_STRING,&luaCommit,luaCommit.size);
        if(sentChunk == 0){
          allLUAparamSent = 1;
        }
        break;
      }

      default: //ID 1
      {
        sentChunk = crsf.sendCRSFparam(CRSF_FRAMETYPE_PARAMETER_SETTINGS_ENTRY,chunk,CRSF_TEXT_SELECTION,&luaAirRate,luaAirRate.size);
        break;
      }
    }
  }
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
  //inital state variables, maybe move elsewhere?
  for (int i = 0; i < 2; i++) // sometimes OpenTX ignores our packets (not sure why yet...)
  {
    sendLuaParams();
    delay(100);
  }
  hwTimer.resume();
}

void ICACHE_RAM_ATTR ParamUpdateReq()
{
  UpdateParamReq = true;
}

void HandleUpdateParameter()
{
  if (millis() > LuaLastUpdated + OPENTX_LUA_UPDATE_INTERVAL)
  {
    sendLuaParams();
    LuaLastUpdated = millis();
  }

  if (UpdateParamReq == false)
  {
    return;
  }

  switch(crsf.ParameterUpdateData[0]){
  case CRSF_FRAMETYPE_PARAMETER_WRITE:
  allLUAparamSent = 0;
    switch (crsf.ParameterUpdateData[1])
    {
    case 0: // special case for sending commit packet
    {
#ifndef DEBUG_SUPPRESS
      Serial.println("send all lua params");
#endif

      
      sendLuaParams();
      break;
    }
    case 1:
      if ((ExpressLRS_currAirRate_Modparams->index != enumRatetoIndex((expresslrs_RFrates_e)crsf.ParameterUpdateData[1])))
      {
#ifndef DEBUG_SUPPRESS
        Serial.print("Request AirRate: ");
        Serial.println(crsf.ParameterUpdateData[1]);
        config.SetRate(enumRatetoIndex((expresslrs_RFrates_e)crsf.ParameterUpdateData[1]));
      #if defined(HAS_OLED)
        OLED.updateScreen(OLED.getPowerString((PowerLevels_e)POWERMGNT.currPower()),
                          OLED.getRateString((expresslrs_RFrates_e)crsf.ParameterUpdateData[1]), 
                          OLED.getTLMRatioString((expresslrs_tlm_ratio_e)(ExpressLRS_currAirRate_Modparams->TLMinterval)), commitStr);
      #endif
      }
      break;

  case 2:
    if ((crsf.ParameterUpdateData[1] <= (uint8_t)TLM_RATIO_1_2) && (crsf.ParameterUpdateData[1] >= (uint8_t)TLM_RATIO_NO_TLM))
    {
      Serial.print("Request TLM interval: ");
      Serial.println(crsf.ParameterUpdateData[1]);
      config.SetTlm((expresslrs_tlm_ratio_e)crsf.ParameterUpdateData[1]);
    #if defined(HAS_OLED)
      OLED.updateScreen(OLED.getPowerString((PowerLevels_e)POWERMGNT.currPower()),
                        OLED.getRateString((expresslrs_RFrates_e)ExpressLRS_currAirRate_Modparams->enum_rate), 
                        OLED.getTLMRatioString((expresslrs_tlm_ratio_e)crsf.ParameterUpdateData[1]), commitStr);
    #endif
    }
    break;

  case 3:
    {
      Serial.print("Request Power: ");
      PowerLevels_e newPower = (PowerLevels_e)crsf.ParameterUpdateData[1];
      Serial.println(newPower, DEC);
      config.SetPower(newPower < MaxPower ? newPower : MaxPower);
      
      #if defined(HAS_OLED)
       OLED.updateScreen(OLED.getPowerString((PowerLevels_e)crsf.ParameterUpdateData[1]),
                         OLED.getRateString((expresslrs_RFrates_e)ExpressLRS_currAirRate_Modparams->enum_rate), 
                         OLED.getTLMRatioString((expresslrs_tlm_ratio_e)ExpressLRS_currAirRate_Modparams->TLMinterval), commitStr);
      #endif
    }
    break;

    case 4:
      if (crsf.ParameterUpdateData[2] == 1)
      {
#ifndef DEBUG_SUPPRESS
        Serial.println("Binding requested from LUA");
#endif
        EnterBindingMode();
      } else if(crsf.ParameterUpdateData[2] == 6){
          sendLuaFieldCrsf(crsf.ParameterUpdateData[1], crsf.ParameterUpdateData[2]);
      }
      else
      {
#ifndef DEBUG_SUPPRESS
        Serial.println("Binding stopped  from LUA");
#endif
        ExitBindingMode();
      }
      break;
      
    case 5:
      if (crsf.ParameterUpdateData[2] == 1)
      {
#ifdef PLATFORM_ESP32
        webUpdateMode = true;
  #ifndef DEBUG_SUPPRESS
        Serial.println("Wifi Update Mode Requested!");
  #endif
        sendLuaParams();
        sendLuaParams();
        BeginWebUpdate();
  #else
        webUpdateMode = false;
  #ifndef DEBUG_SUPPRESS
        Serial.println("Wifi Update Mode Requested but not supported on this platform!");
  #endif
#endif
      } else if(crsf.ParameterUpdateData[2] == 6){
          sendLuaFieldCrsf(crsf.ParameterUpdateData[1],0);
      }
      break;
    case 0x2E:
      suppressCurrentLuaWarning();

      break;
    default:
    break;
    }
  break;

  case CRSF_FRAMETYPE_DEVICE_PING:
  {
    allLUAparamSent = 0;
    updateLUApacketCount();
    crsf.sendCRSFdevice(&luaDevice,luaDevice.size);
    break;
  }
  case CRSF_FRAMETYPE_PARAMETER_READ: //param info
  sendLuaFieldCrsf(crsf.ParameterUpdateData[1],crsf.ParameterUpdateData[2]);
    break;
}

  UpdateParamReq = false;
  if (config.IsModified())
  {
    syncSpamCounter = syncSpamAmount;
  }
}

static void ConfigChangeCommit()
{
  SetRFLinkRate(config.GetRate());
  ExpressLRS_currAirRate_Modparams->TLMinterval = (expresslrs_tlm_ratio_e)config.GetTlm();
  POWERMGNT.setPower((PowerLevels_e)config.GetPower());

  // Write the uncommitted eeprom values
#ifndef DEBUG_SUPPRESS
  Serial.println("EEPROM COMMIT");
#endif
  config.Commit();
  hwTimer.callbackTock = &timerCallbackNormal; // Resume the timer
  resetLuaParams();
  sendLuaParams();
}


static void CheckConfigChangePending()
{
  if (config.IsModified())
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


void setup()
{
#if defined(TARGET_TX_GHOST)
  Serial.setTx(PA2);
  Serial.setRx(PA3);
#endif
  Serial.begin(460800);
#if defined(HAS_OLED)
  OLED.displayLogo();
  OLED.setCommitString(thisCommit, commitStr);
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

#if defined(GPIO_PIN_BUTTON) && (GPIO_PIN_BUTTON != UNDEF_PIN)
  button.init(GPIO_PIN_BUTTON, !GPIO_BUTTON_INVERTED); // r9 tx appears to be active high
#endif

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

#if defined(TARGET_TX_BETAFPV_2400_V1) || defined(TARGET_TX_BETAFPV_900_V1)
  button.buttonTriplePress = &EnterBindingMode;
  button.buttonLongPress = &POWERMGNT.handleCyclePower;
#endif

#ifdef PLATFORM_ESP32
  // Get base mac address
  esp_read_mac(baseMac, ESP_MAC_WIFI_STA);
  // UID[0..2] are OUI (organisationally unique identifier) and are not ESP32 unique.  Do not use!
#endif // PLATFORM_ESP32

  long macSeed = ((long)UID[2] << 24) + ((long)UID[3] << 16) + ((long)UID[4] << 8) + UID[5];
  FHSSrandomiseFHSSsequence(macSeed);

  Radio.RXdoneCallback = &RXdoneISR;
  Radio.TXdoneCallback = &TXdoneISR;

  crsf.connected = &UARTconnected; // it will auto init when it detects UART connection
  crsf.disconnected = &UARTdisconnected;
  crsf.RecvParameterUpdate = &ParamUpdateReq;
  hwTimer.callbackTock = &timerCallbackNormal;
#ifndef DEBUG_SUPPRESS
  Serial.println("ExpressLRS TX Module Booted...");
#endif

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
    BeginWebUpdate();
    while (1)
    {
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
  #ifdef ENABLE_TELEMETRY
  TelemetryReceiver.SetDataToReceive(sizeof(CRSFinBuffer), CRSFinBuffer, ELRS_TELEMETRY_BYTES_PER_CALL);
  #endif

  POWERMGNT.init();

  // Set the pkt rate, TLM ratio, and power from the stored eeprom values
  SetRFLinkRate(config.GetRate());
  ExpressLRS_currAirRate_Modparams->TLMinterval = (expresslrs_tlm_ratio_e)config.GetTlm();
  POWERMGNT.setPower((PowerLevels_e)config.GetPower());
  resetLuaParams();

  hwTimer.init();
  //hwTimer.resume();  //uncomment to automatically start the RX timer and leave it running
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

  updateLEDs(now, connectionState, ExpressLRS_currAirRate_Modparams->index, config.GetPower());

  #if defined(PLATFORM_ESP32)
    if (webUpdateMode)
    {
      HandleWebUpdate();
      return;
    }
  #endif

  HandleUpdateParameter();
  CheckConfigChangePending();

#ifdef FEATURE_OPENTX_SYNC
  // Serial.println(crsf.OpenTXsyncOffset);
  #endif

  if (now > (RX_CONNECTION_LOST_TIMEOUT + LastTLMpacketRecvMillis))
  {
    connectionState = disconnected;
    #if defined(GPIO_PIN_LED_RED) && (GPIO_PIN_LED_RED != UNDEF_PIN)
    digitalWrite(GPIO_PIN_LED_RED, LOW ^ GPIO_LED_RED_INVERTED);
    #endif // GPIO_PIN_LED_RED
  }
  else
  {
    connectionState = connected;
    #if defined(GPIO_PIN_LED_RED) && (GPIO_PIN_LED_RED != UNDEF_PIN)
    digitalWrite(GPIO_PIN_LED_RED, HIGH ^ GPIO_LED_RED_INVERTED);
    #endif // GPIO_PIN_LED_RED
  }

  #ifdef PLATFORM_STM32
    crsf.handleUARTin();
  #endif // PLATFORM_STM32

  #if defined(GPIO_PIN_BUTTON) && (GPIO_PIN_BUTTON != UNDEF_PIN)
    button.handle();
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

  /* Send TLM updates to handset if connected + reporting period
   * is elapsed. This keeps handset happy dispite of the telemetry ratio */
  if ((connectionState == connected) && (LastTLMpacketRecvMillis != 0) &&
      (now >= (uint32_t)(TLM_REPORT_INTERVAL_MS + TLMpacketReported))) {
    crsf.sendLinkStatisticsToTX();
    TLMpacketReported = now;
  }
/* sample packet count only when LUA is not busy, since LUA protocol will interfere packet count*/
//  if ((allLUAparamSent) && (now >= (uint32_t)(LUA_PKTCOUNT_INTERVAL_MS + LUApacketCountReported))){
//      LUApacketCountReported = now;
//      updateLUApacketCount();
//  }

  #ifdef ENABLE_TELEMETRY
  if (TelemetryReceiver.HasFinishedData())
  {
      crsf.sendTelemetryToTX(CRSFinBuffer);
      TelemetryReceiver.Unlock();
  }
  #endif

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
#ifndef DEBUG_SUPPRESS
  Serial.println("TX setpower");
#endif

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
    crsf.AddMspMessage(packet);
  }
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

  InBindingMode = 2;
  setLuaCommandValue(&luaBind,InBindingMode);

  // Start attempting to bind
  // Lock the RF rate and freq while binding
  SetRFLinkRate(RATE_DEFAULT);
  Radio.SetFrequencyReg(GetInitialFreq());
  // Start transmitting again
  hwTimer.resume();

#ifndef DEBUG_SUPPRESS
  Serial.print("Entered binding mode at freq = ");
  Serial.println(Radio.currFreq);
#endif
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

  InBindingMode = 0;
  setLuaCommandValue(&luaBind,InBindingMode);
  MspSender.ResetState();
  SetRFLinkRate(config.GetRate()); //return to original rate

#ifndef DEBUG_SUPPRESS
  Serial.println("Exiting binding mode");
#endif
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
  InBindingMode = 2;
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
