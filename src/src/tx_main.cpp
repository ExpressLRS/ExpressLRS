#include "targets.h"
#include "common.h"

#if defined(Regulatory_Domain_AU_915) || defined(Regulatory_Domain_EU_868) || defined(Regulatory_Domain_FCC_915) || defined(Regulatory_Domain_AU_433) || defined(Regulatory_Domain_EU_433)
#include "SX127xDriver.h"
SX127xDriver Radio;
#elif defined(Regulatory_Domain_ISM_2400)
#include "SX1280Driver.h"
SX1280Driver Radio;
#endif

#include "CRSF.h"
#include "FHSS.h"
#include "LED.h"
// #include "debug.h"
#include "POWERMGNT.h"
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

const uint8_t thisCommit[6] = {LATEST_COMMIT};

//// CONSTANTS ////
#define RX_CONNECTION_LOST_TIMEOUT 3000LU // After 3000ms of no TLM response consider that slave has lost connection
#define MSP_PACKET_SEND_INTERVAL 10LU

#ifndef TLM_REPORT_INTERVAL_MS
#define TLM_REPORT_INTERVAL_MS 320LU // Default to 320ms
#endif

#define LUA_VERSION 3

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

LQCALC<10> LQCalc;
LPF LPD_DownlinkLQ(1);

volatile bool busyTransmitting;
volatile bool UpdateParamReq = false;
uint32_t HWtimerPauseDuration = 0;
#define OPENTX_LUA_UPDATE_INTERVAL 1000
uint32_t LuaLastUpdated = 0;
uint8_t luaCommitPacket[7] = {(uint8_t)0xFE, thisCommit[0], thisCommit[1], thisCommit[2], thisCommit[3], thisCommit[4], thisCommit[5]};

bool WaitRXresponse = false;
bool WaitEepromCommit = false;

bool InBindingMode = false;
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

void ICACHE_RAM_ATTR ProcessTLMpacket()
{
  uint16_t inCRC = (((uint16_t)Radio.RXdataBuffer[0] & 0b11111100) << 6) | Radio.RXdataBuffer[7];

  Radio.RXdataBuffer[0] &= 0b11;
  uint16_t calculatedCRC = ota_crc.calc(Radio.RXdataBuffer, 7, CRCInitializer);

  uint8_t type = Radio.RXdataBuffer[0] & TLM_PACKET;
  uint8_t TLMheader = Radio.RXdataBuffer[1];
  //Serial.println("TLMpacket0");

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
    Serial.println("got downlink conn");
  }

  LastTLMpacketRecvMillis = millis();
  LQCalc.add();

    switch(TLMheader & ELRS_TELEMETRY_TYPE_MASK)
    {
        case ELRS_TELEMETRY_TYPE_LINK:
            // RSSI received is signed, proper polarity (negative value = -dBm)
            crsf.LinkStatistics.uplink_RSSI_1 = Radio.RXdataBuffer[2];
            crsf.LinkStatistics.uplink_RSSI_2 = Radio.RXdataBuffer[3];
            crsf.LinkStatistics.uplink_SNR = Radio.RXdataBuffer[4];
            crsf.LinkStatistics.uplink_Link_quality = Radio.RXdataBuffer[5];
            crsf.LinkStatistics.uplink_TX_Power = POWERMGNT.powerToCrsfPower(POWERMGNT.currPower());
            crsf.LinkStatistics.downlink_SNR = Radio.LastPacketSNR;
            crsf.LinkStatistics.downlink_RSSI = Radio.LastPacketRSSI;
            crsf.LinkStatistics.downlink_Link_quality = LPD_DownlinkLQ.update(LQCalc.getLQ()) + 1; // +1 fixes rounding issues with filter and makes it consistent with RX LQ Calculation
            crsf.LinkStatistics.rf_Mode = (uint8_t)RATE_4HZ - (uint8_t)ExpressLRS_currAirRate_Modparams->enum_rate;
            MspSender.ConfirmCurrentPayload(Radio.RXdataBuffer[6] == 1);
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

  Serial.println("set rate");
  hwTimer.updateInterval(ModParams->interval);
  Radio.Config(ModParams->bw, ModParams->sf, ModParams->cr, GetInitialFreq(), ModParams->PreambleLen, invertIQ);

  ExpressLRS_currAirRate_Modparams = ModParams;
  ExpressLRS_currAirRate_RFperfParams = RFperf;

  crsf.setSyncParams(ModParams->interval);
  connectionState = disconnected;
  rfModeLastChangedMS = millis();

#ifdef PLATFORM_ESP32
  updateLEDs(connectionState, ExpressLRS_currAirRate_Modparams->TLMinterval);
#endif
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

void sendLuaParams()
{
  uint8_t luaParams[] = {0xFF,
                         (uint8_t)(InBindingMode | (webUpdateMode << 1)),
                         (uint8_t)ExpressLRS_currAirRate_Modparams->enum_rate,
                         (uint8_t)(ExpressLRS_currAirRate_Modparams->TLMinterval),
                         (uint8_t)(POWERMGNT.currPower()),
                         (uint8_t)Regulatory_Domain_Index,
                         (uint8_t)crsf.BadPktsCountResult,
                         (uint8_t)((crsf.GoodPktsCountResult & 0xFF00) >> 8),
                         (uint8_t)(crsf.GoodPktsCountResult & 0xFF),
                         (uint8_t)LUA_VERSION};

  crsf.sendLUAresponse(luaParams, 10);
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
#if defined(TARGET_NAMIMNORC_TX)
  WS281BsetLED(0xff, 0, 0);
#endif
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
    crsf.sendLUAresponse(luaCommitPacket, 7);
    delay(100);
    sendLuaParams();
    delay(100);
  }
  hwTimer.resume();
#if defined(TARGET_NAMIMNORC_TX)
  WS281BsetLED(0, 0xff, 0);
#endif
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

  switch (crsf.ParameterUpdateData[0])
  {
  case 0: // special case for sending commit packet
    Serial.println("send all lua params");
    crsf.sendLUAresponse(luaCommitPacket, 7);
    break;

  case 1:
    if ((ExpressLRS_currAirRate_Modparams->index != enumRatetoIndex((expresslrs_RFrates_e)crsf.ParameterUpdateData[1])))
    {
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
    Serial.print("Request Power: ");
    Serial.println(crsf.ParameterUpdateData[1]);
    if((PowerLevels_e)crsf.ParameterUpdateData[1] <= MaxPower){
      config.SetPower((PowerLevels_e)crsf.ParameterUpdateData[1]);
      #if defined(HAS_OLED)
        OLED.updateScreen(OLED.getPowerString((PowerLevels_e)crsf.ParameterUpdateData[1]),
                          OLED.getRateString((expresslrs_RFrates_e)ExpressLRS_currAirRate_Modparams->enum_rate), 
                          OLED.getTLMRatioString((expresslrs_tlm_ratio_e)ExpressLRS_currAirRate_Modparams->TLMinterval), commitStr);
      #endif
    }
    break;

  case 4:
    break;

  case 0xFE:
    if (crsf.ParameterUpdateData[1] == 1)
    {
#ifdef PLATFORM_ESP32
      webUpdateMode = true;
      Serial.println("Wifi Update Mode Requested!");
      sendLuaParams();
      sendLuaParams();
      BeginWebUpdate();
#else
      webUpdateMode = false;
      Serial.println("Wifi Update Mode Requested but not supported on this platform!");
#endif
      break;
    }

  case 0xFF:
    if (crsf.ParameterUpdateData[1] == 1)
    {
      Serial.println("Binding requested from LUA");
      EnterBindingMode();
    }
    else
    {
      Serial.println("Binding stopped  from LUA");
      ExitBindingMode();
    }
    break;

  default:
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
  Serial.println("EEPROM COMMIT");
  config.Commit();
  hwTimer.callbackTock = &timerCallbackNormal; // Resume the timer
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

  /**
   * Any TX's that have the WS2812 LED will use this the WS2812 LED pin
   * else we will use GPIO_PIN_LED_GREEN and _RED.
   **/
  #if WS2812_LED_IS_USED // do startup blinkies for fun
      WS281Binit();
      uint32_t col = 0x0000FF;
      for (uint8_t j = 0; j < 3; j++)
      {
          for (uint8_t i = 0; i < 5; i++)
          {
              WS281BsetLED(col << j*8);
              delay(15);
              WS281BsetLED(0, 0, 0);
              delay(35);
          }
      }
      WS281BsetLED(0xff, 0, 0);
  #endif

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
#ifdef GPIO_PIN_LED
  strip.Begin();
#endif
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

  Serial.println("ExpressLRS TX Module Booted...");

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

  eeprom.Begin(); // Init the eeprom
  config.SetStorageProvider(&eeprom); // Pass pointer to the Config class for access to storage
  config.Load(); // Load the stored values from eeprom

  // Set the pkt rate, TLM ratio, and power from the stored eeprom values
  SetRFLinkRate(config.GetRate());
  ExpressLRS_currAirRate_Modparams->TLMinterval = (expresslrs_tlm_ratio_e)config.GetTlm();
  POWERMGNT.setPower((PowerLevels_e)config.GetPower());
  

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
  #if WS2812_LED_IS_USED && !defined(TARGET_NAMIMNORC_TX)
      if ((connectionState == disconnected) && (now > (LEDupdateCounterMillis + LEDupdateInterval)))
      {
          uint8_t LEDcolor[3] = {0};
          if (LEDfade == 30 || LEDfade == 0)
          {
              LEDfadeDir = !LEDfadeDir;
          }

          LEDfadeDir ? LEDfade = LEDfade + 2 :  LEDfade = LEDfade - 2;
          LEDcolor[(2 - ExpressLRS_currAirRate_Modparams->index) % 3] = LEDfade;
          WS281BsetLED(LEDcolor);
          LEDupdateCounterMillis = now;
      }
  #endif

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

  #ifdef ENABLE_TELEMETRY
  if (TelemetryReceiver.HasFinishedData())
  {
      crsf.sendTelemetryToTX(CRSFinBuffer);
      TelemetryReceiver.Unlock();
  }
  #endif

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
  Serial.println("TX setpower");

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
  InBindingMode = true;

  // Start attempting to bind
  // Lock the RF rate and freq while binding
  SetRFLinkRate(RATE_DEFAULT);
  Radio.SetFrequencyReg(GetInitialFreq());
  // Start transmitting again
  hwTimer.resume();

  Serial.print("Entered binding mode at freq = ");
  Serial.println(Radio.currFreq);
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

  Serial.println("Exiting binding mode");
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
