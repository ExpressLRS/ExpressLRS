#include <Arduino.h>
#include "FIFO.h"
#include "utils.h"
#include "common.h"

#if defined(Regulatory_Domain_AU_915) || defined(Regulatory_Domain_EU_868) || defined(Regulatory_Domain_FCC_915) || defined(Regulatory_Domain_AU_433) || defined(Regulatory_Domain_EU_433)
#include "SX127xDriver.h"
SX127xDriver Radio;
#elif Regulatory_Domain_ISM_2400
#include "SX1280Driver.h"
SX1280Driver Radio;
#endif

#include "CRSF.h"
#include "FHSS.h"
#include "LED.h"
// #include "debug.h"
#include "targets.h"
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

#ifdef PLATFORM_ESP8266
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#endif

#ifdef PLATFORM_ESP32
#include "ESP32_WebUpdate.h"
#endif

#if defined(TARGET_R9M_TX) || defined(TARGET_TX_ES915TX) || defined(TARGET_NAMIMNO_ALPHA_TX)
#include "DAC.h"
DAC TxDAC;
#endif
#if defined(GPIO_PIN_BUTTON) && (GPIO_PIN_BUTTON != UNDEF_PIN)
#include "button.h"
button button;
#endif

#if (GPIO_PIN_LED_WS2812 != UNDEF_PIN) && (GPIO_PIN_LED_WS2812_FAST != UNDEF_PIN)
uint8_t LEDfadeDiv;
uint8_t LEDfade;
bool LEDfadeDir;
uint32_t LEDupdateInterval = 25;
uint32_t LEDupdateCounterMillis;
#include "STM32F3_WS2812B_LED.h"
#endif

const uint8_t thisCommit[6] = {LATEST_COMMIT};

//// CONSTANTS ////
#define RX_CONNECTION_LOST_TIMEOUT 3000LU // After 3000ms of no TLM response consider that slave has lost connection
#define MSP_PACKET_SEND_INTERVAL 200LU

#ifndef TLM_REPORT_INTERVAL_MS
#define TLM_REPORT_INTERVAL_MS 320LU // Default to 320ms
#endif

/// define some libs to use ///
hwTimer hwTimer;
GENERIC_CRC8 ota_crc(ELRS_CRC_POLY);
CRSF crsf;
POWERMGNT POWERMGNT;
MSP msp;
ELRS_EEPROM eeprom;
TxConfig config;

void ICACHE_RAM_ATTR TimerCallbackISR();
volatile uint8_t NonceTX;

bool webUpdateMode = false;

//// MSP Data Handling ///////
uint32_t MSPPacketLastSent = 0;  // time in ms when the last switch data packet was sent
uint32_t MSPPacketSendCount = 0; // number of times to send MSP packet
mspPacket_t MSPPacket;

////////////SYNC PACKET/////////
uint32_t SyncPacketLastSent = 0;

volatile uint32_t LastTLMpacketRecvMillis = 0;
uint32_t TLMpacketReported = 0;

LQCALC LQCALC;
LPF LPD_DownlinkLQ(1);

volatile bool UpdateParamReq = false;
#define OPENTX_LUA_UPDATE_INTERVAL 1000
uint32_t LuaLastUpdated = 0;
uint8_t luaCommitPacket[7] = {(uint8_t)0xFE, thisCommit[0], thisCommit[1], thisCommit[2], thisCommit[3], thisCommit[4], thisCommit[5]};

uint32_t PacketLastSentMicros = 0;

bool Channels5to8Changed = false;

bool WaitRXresponse = false;
bool WaitEepromCommit = false;

bool InBindingMode = false;
void EnterBindingMode();
void ExitBindingMode();
void SendUIDOverMSP();

#ifdef ENABLE_TELEMETRY
StubbornReceiver TelemetryReceiver(ELRS_TELEMETRY_MAX_PACKAGES);
#endif
uint8_t CRSFinBuffer[CRSF_MAX_PACKET_LEN];
// MSP packet handling function defs
void ProcessMSPPacket(mspPacket_t *packet);
void OnRFModePacket(mspPacket_t *packet);
void OnTxPowerPacket(mspPacket_t *packet);
void OnTLMRatePacket(mspPacket_t *packet);

uint8_t baseMac[6];

void ICACHE_RAM_ATTR ProcessTLMpacket()
{
  uint8_t calculatedCRC = ota_crc.calc(Radio.RXdataBuffer, 7) + CRCCaesarCipher;
  uint8_t inCRC = Radio.RXdataBuffer[7];
  uint8_t type = Radio.RXdataBuffer[0] & TLM_PACKET;
  uint8_t packetAddr = (Radio.RXdataBuffer[0] & 0b11111100) >> 2;
  uint8_t TLMheader = Radio.RXdataBuffer[1];
  //Serial.println("TLMpacket0");

  if (packetAddr != DeviceAddr)
  {
#ifndef DEBUG_SUPPRESS
    Serial.println("TLM device address error");
#endif
    return;
  }

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
  LQCALC.add();

    switch(TLMheader & ELRS_TELEMETRY_TYPE_MASK)
    {
        case ELRS_TELEMETRY_TYPE_LINK:
            crsf.LinkStatistics.uplink_RSSI_1 = Radio.RXdataBuffer[2];
            crsf.LinkStatistics.uplink_RSSI_2 = 0;
            crsf.LinkStatistics.uplink_SNR = Radio.RXdataBuffer[4];
            crsf.LinkStatistics.uplink_Link_quality = Radio.RXdataBuffer[5];

            crsf.LinkStatistics.downlink_SNR = int(Radio.LastPacketSNR * 10);
            crsf.LinkStatistics.downlink_RSSI = 120 + Radio.LastPacketRSSI;
            crsf.LinkStatistics.downlink_Link_quality = LPD_DownlinkLQ.update(LQCALC.getLQ()) + 1; // +1 fixes rounding issues with filter and makes it consistent with RX LQ Calculation
            //crsf.LinkStatistics.downlink_Link_quality = Radio.currPWR;
            crsf.LinkStatistics.rf_Mode = 4 - ExpressLRS_currAirRate_Modparams->index;
            break;

        #ifdef ENABLE_TELEMETRY
        case ELRS_TELEMETRY_TYPE_DATA:
            TelemetryReceiver.ReceiveData(TLMheader >> ELRS_TELEMETRY_SHIFT, Radio.RXdataBuffer + 2);
            if (TelemetryReceiver.HasFinishedData())
            {
                crsf.sendTelemetryToTX(CRSFinBuffer);
                TelemetryReceiver.Unlock();
            }
            break;
        #endif
    }
}

void ICACHE_RAM_ATTR CheckChannels5to8Change()
{ //check if channels 5 to 8 have new data (switch channels)
  for (int i = 4; i < 8; i++)
  {
    if (crsf.ChannelDataInPrev[i] != crsf.ChannelDataIn[i])
    {
      Channels5to8Changed = true;
    }
  }
}

void ICACHE_RAM_ATTR GenerateSyncPacketData()
{
  uint8_t PacketHeaderAddr;
#ifdef HYBRID_SWITCHES_8
  uint8_t SwitchEncMode = 0b01;
#else
  uint8_t SwitchEncMode = 0b00;
#endif
  uint8_t Index = (ExpressLRS_currAirRate_Modparams->index & 0b11);
  uint8_t TLMrate = (ExpressLRS_currAirRate_Modparams->TLMinterval & 0b111);
  PacketHeaderAddr = (DeviceAddr << 2) + SYNC_PACKET;
  Radio.TXdataBuffer[0] = PacketHeaderAddr;
  Radio.TXdataBuffer[1] = FHSSgetCurrIndex();
  Radio.TXdataBuffer[2] = NonceTX;
  Radio.TXdataBuffer[3] = (Index << 6) + (TLMrate << 3) + (SwitchEncMode << 1);
  Radio.TXdataBuffer[4] = UID[3];
  Radio.TXdataBuffer[5] = UID[4];
  Radio.TXdataBuffer[6] = UID[5];
}

void ICACHE_RAM_ATTR Generate4ChannelData_10bit()
{
  uint8_t PacketHeaderAddr;
  PacketHeaderAddr = (DeviceAddr << 2) + RC_DATA_PACKET;
  Radio.TXdataBuffer[0] = PacketHeaderAddr;
  Radio.TXdataBuffer[1] = ((CRSF_to_UINT10(crsf.ChannelDataIn[0]) & 0b1111111100) >> 2);
  Radio.TXdataBuffer[2] = ((CRSF_to_UINT10(crsf.ChannelDataIn[1]) & 0b1111111100) >> 2);
  Radio.TXdataBuffer[3] = ((CRSF_to_UINT10(crsf.ChannelDataIn[2]) & 0b1111111100) >> 2);
  Radio.TXdataBuffer[4] = ((CRSF_to_UINT10(crsf.ChannelDataIn[3]) & 0b1111111100) >> 2);
  Radio.TXdataBuffer[5] = ((CRSF_to_UINT10(crsf.ChannelDataIn[0]) & 0b0000000011) << 6) +
                          ((CRSF_to_UINT10(crsf.ChannelDataIn[1]) & 0b0000000011) << 4) +
                          ((CRSF_to_UINT10(crsf.ChannelDataIn[2]) & 0b0000000011) << 2) +
                          ((CRSF_to_UINT10(crsf.ChannelDataIn[3]) & 0b0000000011) << 0);
}

void ICACHE_RAM_ATTR Generate4ChannelData_11bit()
{
  uint8_t PacketHeaderAddr;
  PacketHeaderAddr = (DeviceAddr << 2) + RC_DATA_PACKET;
  Radio.TXdataBuffer[0] = PacketHeaderAddr;
  Radio.TXdataBuffer[1] = ((crsf.ChannelDataIn[0]) >> 3);
  Radio.TXdataBuffer[2] = ((crsf.ChannelDataIn[1]) >> 3);
  Radio.TXdataBuffer[3] = ((crsf.ChannelDataIn[2]) >> 3);
  Radio.TXdataBuffer[4] = ((crsf.ChannelDataIn[3]) >> 3);
  Radio.TXdataBuffer[5] = ((crsf.ChannelDataIn[0] & 0b00000111) << 5) +
                          ((crsf.ChannelDataIn[1] & 0b111) << 2) +
                          ((crsf.ChannelDataIn[2] & 0b110) >> 1);
  Radio.TXdataBuffer[6] = ((crsf.ChannelDataIn[2] & 0b001) << 7) +
                          ((crsf.ChannelDataIn[3] & 0b111) << 4); // 4 bits left over for something else?
#ifdef One_Bit_Switches
  Radio.TXdataBuffer[6] += CRSF_to_BIT(crsf.ChannelDataIn[4]) << 3;
  Radio.TXdataBuffer[6] += CRSF_to_BIT(crsf.ChannelDataIn[5]) << 2;
  Radio.TXdataBuffer[6] += CRSF_to_BIT(crsf.ChannelDataIn[6]) << 1;
  Radio.TXdataBuffer[6] += CRSF_to_BIT(crsf.ChannelDataIn[7]) << 0;
#endif
#ifdef ENABLE_TELEMETRY
  Radio.TXdataBuffer[6] =  Radio.TXdataBuffer[6] & (~1 | TelemetryReceiver.GetCurrentConfirm());
#endif
}

void ICACHE_RAM_ATTR GenerateMSPData()
{
  uint8_t PacketHeaderAddr;
  PacketHeaderAddr = (DeviceAddr << 2) + MSP_DATA_PACKET;
  Radio.TXdataBuffer[0] = PacketHeaderAddr;
  Radio.TXdataBuffer[1] = MSPPacket.function;
  Radio.TXdataBuffer[2] = MSPPacket.payloadSize;
  Radio.TXdataBuffer[3] = 0;
  Radio.TXdataBuffer[4] = 0;
  Radio.TXdataBuffer[5] = 0;
  Radio.TXdataBuffer[6] = 0;
  if (MSPPacket.payloadSize <= 4)
  {
    MSPPacket.payloadReadIterator = 0;
    for (int i = 0; i < MSPPacket.payloadSize; i++)
    {
      Radio.TXdataBuffer[3 + i] = MSPPacket.readByte();
    }
  }
  else
  {
    Serial.println("Unable to send MSP command. Packet too long.");
  }
}

void ICACHE_RAM_ATTR SetRFLinkRate(uint8_t index) // Set speed of RF link (hz)
{
  Serial.println("set rate");
  expresslrs_mod_settings_s *const ModParams = get_elrs_airRateConfig(index);
  expresslrs_rf_pref_params_s *const RFperf = get_elrs_RFperfParams(index);

  Radio.Config(ModParams->bw, ModParams->sf, ModParams->cr, GetInitialFreq(), ModParams->PreambleLen);
  hwTimer.updateInterval(ModParams->interval);

  ExpressLRS_currAirRate_Modparams = ModParams;
  ExpressLRS_currAirRate_RFperfParams = RFperf;

  crsf.setSyncParams(ModParams->interval);
  connectionState = connected;

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
        LQCALC.inc();
        return;
      }
      else
      {
        NonceTX++;
      }
    }
  }

  uint32_t SyncInterval;

#if defined(NO_SYNC_ON_ARM) && defined(ARM_CHANNEL)
  SyncInterval = 250;
  bool skipSync = (bool)CRSF_to_BIT(crsf.ChannelDataIn[ARM_CHANNEL - 1]);
#else
  SyncInterval = (connectionState == connected) ? ExpressLRS_currAirRate_RFperfParams->SyncPktIntervalConnected : ExpressLRS_currAirRate_RFperfParams->SyncPktIntervalDisconnected;
  bool skipSync = false;
#endif

  if ((!skipSync) && ((millis() > (SyncPacketLastSent + SyncInterval)) && (Radio.currFreq == GetInitialFreq()) && ((NonceTX) % ExpressLRS_currAirRate_Modparams->FHSShopInterval == 0))) // sync just after we changed freqs (helps with hwTimer.init() being in sync from the get go)
  {

    GenerateSyncPacketData();
    SyncPacketLastSent = millis();
    //Serial.println("sync");
    //Serial.println(Radio.currFreq);
  }
  else
  {
    if ((millis() > (MSP_PACKET_SEND_INTERVAL + MSPPacketLastSent)) && MSPPacketSendCount)
    {
      GenerateMSPData();
      MSPPacketLastSent = millis();
      MSPPacketSendCount--;

      if (MSPPacketSendCount <= 0 && InBindingMode)
      {
        ExitBindingMode();
      }
    }
    else
    {
      #if defined HYBRID_SWITCHES_8
      #ifdef ENABLE_TELEMETRY
      GenerateChannelDataHybridSwitch8(Radio.TXdataBuffer, &crsf, DeviceAddr, TelemetryReceiver.GetCurrentConfirm());
      #else
      GenerateChannelDataHybridSwitch8(Radio.TXdataBuffer, &crsf, DeviceAddr, false);
      #endif
      #elif defined SEQ_SWITCHES
      GenerateChannelDataSeqSwitch(Radio.TXdataBuffer, &crsf, DeviceAddr);
      #else
      Generate4ChannelData_11bit();
      #endif
    }
  }

  ///// Next, Calculate the CRC and put it into the buffer /////
  uint8_t crc = ota_crc.calc(Radio.TXdataBuffer, 7) + CRCCaesarCipher;
  Radio.TXdataBuffer[7] = crc;
  Radio.TXnb(Radio.TXdataBuffer, 8);
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
                         (uint8_t)(crsf.GoodPktsCountResult & 0xFF)};

  crsf.sendLUAresponse(luaParams, 9);
}

void UARTdisconnected()
{
  #ifdef GPIO_PIN_BUZZER
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
  #ifdef GPIO_PIN_BUZZER
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
      ExpressLRS_nextAirRateIndex = enumRatetoIndex((expresslrs_RFrates_e)crsf.ParameterUpdateData[1]);
      config.SetRate(ExpressLRS_nextAirRateIndex);
    }
    break;

  case 2:
    if ((crsf.ParameterUpdateData[1] <= (uint8_t)TLM_RATIO_1_2) && (crsf.ParameterUpdateData[1] >= (uint8_t)TLM_RATIO_NO_TLM))
    {
      Serial.print("Request TLM interval: ");
      Serial.println(ExpressLRS_currAirRate_Modparams->TLMinterval);
      config.SetTlm((expresslrs_tlm_ratio_e)crsf.ParameterUpdateData[1]);
    }
    break;

  case 3:
    Serial.print("Request Power: ");
    Serial.println(crsf.ParameterUpdateData[1]);
    config.SetPower((PowerLevels_e)crsf.ParameterUpdateData[1]);
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
      delay(500);
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
    // Stop the timer during eeprom writes
    hwTimer.stop();
    // Set a flag that will trigger the eeprom commit in the main loop
    // NOTE: This is required to ensure we wait long enough for any outstanding IRQ's to fire
    WaitEepromCommit = true;
  }
}

void ICACHE_RAM_ATTR RXdoneISR()
{
  ProcessTLMpacket();
}

void ICACHE_RAM_ATTR TXdoneISR()
{
  NonceTX++; // must be done before callback
  HandleFHSS();
  HandleTLM();
}

void setup()
{
#ifdef TARGET_TX_GHOST
  Serial.setTx(PA2);
  Serial.setRx(PA3);
#endif
  Serial.begin(460800);

  /**
   * Any TX's that have the WS2812 LED will use this the WS2812 LED pin
   * else we will use GPIO_PIN_LED_GREEN and _RED.
   **/
  #if WS2812_LED_IS_USED // do startup blinkies for fun
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
  #endif

  #if defined(GPIO_PIN_LED_GREEN) && (GPIO_PIN_LED_GREEN != UNDEF_PIN)
    pinMode(GPIO_PIN_LED_GREEN, OUTPUT);
    digitalWrite(GPIO_PIN_LED_GREEN, HIGH);
  #endif // GPIO_PIN_LED_GREEN
  #if defined(GPIO_PIN_LED_RED) && (GPIO_PIN_LED_RED != UNDEF_PIN)
    pinMode(GPIO_PIN_LED_RED, OUTPUT);
  #endif // GPIO_PIN_LED_RED

  #if defined(GPIO_PIN_BUZZER) && (GPIO_PIN_BUZZER != UNDEF_PIN)
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

  #if defined(TARGET_R9M_TX) || defined(TARGET_TX_ES915TX) || defined(TARGET_NAMIMNO_ALPHA_TX)
    TxDAC.init();
  #endif


#if defined(GPIO_PIN_BUTTON) && (GPIO_PIN_BUTTON != UNDEF_PIN)
  button.init(GPIO_PIN_BUTTON, true); // r9 tx appears to be active high
#endif

#ifdef PLATFORM_ESP32
#ifdef GPIO_PIN_LED
  strip.Begin();
#endif
  // Get base mac address
  esp_read_mac(baseMac, ESP_MAC_WIFI_STA);
  // Print base mac address
  // This should be copied to common.h and is used to generate a unique hop sequence, DeviceAddr, and CRC.
  // UID[0..2] are OUI (organisationally unique identifier) and are not ESP32 unique.  Do not use!
#endif // PLATFORM_ESP32

  FHSSrandomiseFHSSsequence();

  Radio.RXdoneCallback = &RXdoneISR;
  Radio.TXdoneCallback = &TXdoneISR;

#ifndef One_Bit_Switches
  crsf.RCdataCallback1 = &CheckChannels5to8Change;
#endif
  crsf.connected = &UARTconnected; // it will auto init when it detects UART connection
  crsf.disconnected = &UARTdisconnected;
  crsf.RecvParameterUpdate = &ParamUpdateReq;
  hwTimer.callbackTock = &TimerCallbackISR;

  Serial.println("ExpressLRS TX Module Booted...");

  POWERMGNT.init();
  Radio.currFreq = GetInitialFreq(); //set frequency first or an error will occur!!!
  #if !defined(Regulatory_Domain_ISM_2400)
  //Radio.currSyncWord = UID[3];
  #endif
  bool init_success = Radio.Begin();
  while (!init_success)
  {
    #if defined(GPIO_PIN_LED_GREEN) && (GPIO_PIN_LED_GREEN != UNDEF_PIN)
      digitalWrite(GPIO_PIN_LED_GREEN, LOW);
    #endif // GPIO_PIN_LED_GREEN
    #if defined(GPIO_PIN_BUZZER) && (GPIO_PIN_BUZZER != UNDEF_PIN)
      tone(GPIO_PIN_BUZZER, 480, 200);
    #endif // GPIO_PIN_BUZZER
    #if defined(GPIO_PIN_LED_RED) && (GPIO_PIN_LED_RED != UNDEF_PIN)
      digitalWrite(GPIO_PIN_LED_RED, LOW);
    #endif // GPIO_PIN_LED_RED
    delay(200);
    #if defined(GPIO_PIN_BUZZER) && (GPIO_PIN_BUZZER != UNDEF_PIN)
      tone(GPIO_PIN_BUZZER, 400, 200);
    #endif // GPIO_PIN_BUZZER
    #if defined(GPIO_PIN_LED_RED) && (GPIO_PIN_LED_RED != UNDEF_PIN)
      digitalWrite(GPIO_PIN_LED_RED, HIGH);
    #endif // GPIO_PIN_LED_RED
    delay(1000);
  }
  #ifdef ENABLE_TELEMETRY
  TelemetryReceiver.ResetState();
  TelemetryReceiver.SetDataToReceive(sizeof(CRSFinBuffer), CRSFinBuffer, ELRS_TELEMETRY_BYTES_PER_CALL);
  #endif
  POWERMGNT.setDefaultPower();

  eeprom.Begin(); // Init the eeprom
  config.SetStorageProvider(&eeprom); // Pass pointer to the Config class for access to storage
  config.Load(); // Load the stored values from eeprom

  // Set the pkt rate, TLM ratio, and power from the stored eeprom values
  SetRFLinkRate(config.GetRate());
  ExpressLRS_nextAirRateIndex = ExpressLRS_currAirRate_Modparams->index;
  ExpressLRS_currAirRate_Modparams->TLMinterval = (expresslrs_tlm_ratio_e)config.GetTlm();
  POWERMGNT.setPower((PowerLevels_e)config.GetPower());

  crsf.Begin();
  hwTimer.init();
  hwTimer.resume();
  hwTimer.stop(); //comment to automatically start the RX timer and leave it running
  LQCALC.init(10);
}

void loop()
{
  uint32_t now = millis();

  #if WS2812_LED_IS_USED
      if ((connectionState == disconnected) && (now > LEDupdateInterval + LEDupdateCounterMillis))
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

  // If there's an outstanding eeprom write, and we've waited long enough for any IRQs to fire...
  if (WaitEepromCommit && (micros() - PacketLastSentMicros) > ExpressLRS_currAirRate_Modparams->interval)
  {
    SetRFLinkRate(config.GetRate());
    ExpressLRS_currAirRate_Modparams->TLMinterval = (expresslrs_tlm_ratio_e)config.GetTlm();
    POWERMGNT.setPower((PowerLevels_e)config.GetPower());

    // Write the values, and restart the timer
    WaitEepromCommit = false;
    // Write the uncommitted eeprom values
    Serial.println("EEPROM COMMIT");
    config.Commit();
    // Resume the timer
    sendLuaParams();
    hwTimer.resume();
  }

  #ifdef FEATURE_OPENTX_SYNC
  // Serial.println(crsf.OpenTXsyncOffset);
  #endif

  if (now > (RX_CONNECTION_LOST_TIMEOUT + LastTLMpacketRecvMillis))
  {
    connectionState = disconnected;
    #if defined(GPIO_PIN_LED_RED) && (GPIO_PIN_LED_RED != UNDEF_PIN)
    digitalWrite(GPIO_PIN_LED_RED, LOW);
    #endif // GPIO_PIN_LED_RED
  }
  else
  {
    connectionState = connected;
    #if defined(GPIO_PIN_LED_RED) && (GPIO_PIN_LED_RED != UNDEF_PIN)
    digitalWrite(GPIO_PIN_LED_RED, HIGH);
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
}

void ICACHE_RAM_ATTR TimerCallbackISR()
{
  SendRCdataToRF();
  PacketLastSentMicros = micros();
}

void OnRFModePacket(mspPacket_t *packet)
{
  // Parse the RF mode
  uint8_t rfMode = packet->readByte();
  CHECK_PACKET_PARSING();

  switch (rfMode)
  {
  case RATE_200HZ:
    SetRFLinkRate(enumRatetoIndex(RATE_200HZ));
    break;
  case RATE_100HZ:
    SetRFLinkRate(enumRatetoIndex(RATE_100HZ));
    break;
  case RATE_50HZ:
    SetRFLinkRate(enumRatetoIndex(RATE_50HZ));
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

  switch (txPower)
  {
  case PWR_10mW:
    POWERMGNT.setPower(PWR_10mW);
    break;
  case PWR_25mW:
    POWERMGNT.setPower(PWR_25mW);
    break;
  case PWR_50mW:
    POWERMGNT.setPower(PWR_50mW);
    break;
  case PWR_100mW:
    POWERMGNT.setPower(PWR_100mW);
    break;
  case PWR_250mW:
    POWERMGNT.setPower(PWR_250mW);
    break;
  case PWR_500mW:
    POWERMGNT.setPower(PWR_500mW);
    break;
  case PWR_1000mW:
    POWERMGNT.setPower(PWR_1000mW);
    break;
  case PWR_2000mW:
    POWERMGNT.setPower(PWR_2000mW);
    break;
  default:
    // Unsupported power requested
    break;
  }
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
    MSPPacket = *packet;
    MSPPacketSendCount = 6;
  }
}

void EnterBindingMode()
{
  if (InBindingMode) {
      // Don't enter binding if we're already binding
      return;
  }

  // Start periodically sending the current UID as MSP packets
  SendUIDOverMSP();

  // Set UID to special binding values
  UID[0] = BindingUID[0];
  UID[1] = BindingUID[1];
  UID[2] = BindingUID[2];
  UID[3] = BindingUID[3];
  UID[4] = BindingUID[4];
  UID[5] = BindingUID[5];

  CRCCaesarCipher = UID[4];
  DeviceAddr = UID[5] & 0b111111;

  InBindingMode = true;

  // Start attempting to bind
  // Lock the RF rate and freq while binding
  SetRFLinkRate(RATE_DEFAULT);
  Radio.SetFrequencyReg(GetInitialFreq());
  POWERMGNT.setPower(PWR_10mW);

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

  CRCCaesarCipher = UID[4];
  DeviceAddr = UID[5] & 0b111111;

  InBindingMode = false;

  Serial.println("Exiting binding mode");
}

void SendUIDOverMSP()
{
  MSPPacket.reset();

  MSPPacket.makeCommand();
  MSPPacket.function = MSP_ELRS_BIND;
  MSPPacket.addByte(UID[2]);
  MSPPacket.addByte(UID[3]);
  MSPPacket.addByte(UID[4]);
  MSPPacket.addByte(UID[5]);

  MSPPacketSendCount = 10;
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
