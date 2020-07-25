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
//#include "elrs_eeprom.h"

#ifdef PLATFORM_ESP8266
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#endif

#ifdef PLATFORM_ESP32
#include "ESP32_hwTimer.h"
#endif

#ifdef TARGET_R9M_TX
#include "DAC.h"
#include "STM32_hwTimer.h"
#include "button.h"
button button;
R9DAC R9DAC;
#endif

#ifdef TARGET_R9M_LITE_TX
#include "STM32_hwTimer.h"
#endif

uint8_t thisCommit[6] = {LATEST_COMMIT};

//// CONSTANTS ////
#define RX_CONNECTION_LOST_TIMEOUT 3000 // After 1500ms of no TLM response consider that slave has lost connection
#define PACKET_RATE_INTERVAL 500
#define RF_MODE_CYCLE_INTERVAL 1000
#define MSP_PACKET_SEND_INTERVAL 200
#define SYNC_PACKET_SEND_INTERVAL_RX_LOST 1000 // how often to send the SYNC_PACKET packet (ms) when there is no response from RX
#define SYNC_PACKET_SEND_INTERVAL_RX_CONN 5000 // how often to send the SYNC_PACKET packet (ms) when there we have a connection

String DebugOutput;

/// define some libs to use ///
hwTimer hwTimer;
CRSF crsf;
POWERMGNT POWERMGNT;
MSP msp;

void ICACHE_RAM_ATTR TimerCallbackISR();
volatile uint8_t NonceTX;

//// MSP Data Handling ///////
uint32_t MSPPacketLastSent = 0;  // time in ms when the last switch data packet was sent
uint32_t MSPPacketSendCount = 0; // number of times to send MSP packet
mspPacket_t MSPPacket;

////////////SYNC PACKET/////////
uint32_t SyncPacketLastSent = 0;

uint32_t LastTLMpacketRecvMillis = 0;
bool isRXconnected = false;
int packetCounteRX_TX = 0;
uint32_t PacketRateLastChecked = 0;
float PacketRate = 0.0;
uint8_t linkQuality = 0;

/// Variables for Sync Behaviour ////
uint32_t RFmodeLastCycled = 0;
///////////////////////////////////////

volatile bool UpdateParamReq = false;
volatile bool UpdateRFparamReq = false;

volatile bool RadioIsIdle = false;

bool Channels5to8Changed = false;

bool ChangeAirRateRequested = false;
bool ChangeAirRateSentUpdate = false;

bool WaitRXresponse = false;

///// Not used in this version /////////////////////////////////////////////////////////////////////////////////////////////////////////
uint8_t TelemetryWaitBuffer[7] = {0};

uint32_t LinkSpeedIncreaseDelayFactor = 500; // wait for the connection to be 'good' for this long before increasing the speed.
uint32_t LinkSpeedDecreaseDelayFactor = 200; // this long wait this long for connection to be below threshold before dropping speed

uint32_t LinkSpeedDecreaseFirstMetCondition = 0;
uint32_t LinkSpeedIncreaseFirstMetCondition = 0;

uint8_t LinkSpeedReduceSNR = 20;   //if the SNR (times 10) is lower than this we drop the link speed one level
uint8_t LinkSpeedIncreaseSNR = 60; //if the SNR (times 10) is higher than this we increase the link speed
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// MSP packet handling function defs
void ProcessMSPPacket(mspPacket_t *packet);
void OnRFModePacket(mspPacket_t *packet);
void OnTxPowerPacket(mspPacket_t *packet);
void OnTLMRatePacket(mspPacket_t *packet);

uint8_t baseMac[6];

void ICACHE_RAM_ATTR ProcessTLMpacket()
{
  uint8_t calculatedCRC = CalcCRC(Radio.RXdataBuffer, 7) + CRCCaesarCipher;
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

  packetCounteRX_TX++;

  if (type != TLM_PACKET)
  {
#ifndef DEBUG_SUPPRESS
    Serial.println("TLM type error");
    Serial.println(type);
#endif
    return;
  }

  isRXconnected = true;
  LastTLMpacketRecvMillis = millis();

  if (TLMheader == CRSF_FRAMETYPE_LINK_STATISTICS)
  {
    crsf.LinkStatistics.uplink_RSSI_1 = Radio.RXdataBuffer[2];
    crsf.LinkStatistics.uplink_RSSI_2 = 0;
    crsf.LinkStatistics.uplink_SNR = Radio.RXdataBuffer[4];
    crsf.LinkStatistics.uplink_Link_quality = Radio.RXdataBuffer[5];

    crsf.LinkStatistics.downlink_SNR = int(Radio.LastPacketSNR * 10);
    crsf.LinkStatistics.downlink_RSSI = 120 + Radio.LastPacketRSSI;
    crsf.LinkStatistics.downlink_Link_quality = linkQuality;
    //crsf.LinkStatistics.downlink_Link_quality = Radio.currPWR;
    crsf.LinkStatistics.rf_Mode = 4 - ExpressLRS_currAirRate_Modparams->index;

    crsf.TLMbattSensor.voltage = (Radio.RXdataBuffer[3] << 8) + Radio.RXdataBuffer[6];

    crsf.sendLinkStatisticsToTX();
    crsf.sendLinkBattSensorToTX();
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
  PacketHeaderAddr = (DeviceAddr << 2) + SYNC_PACKET;
  Radio.TXdataBuffer[0] = PacketHeaderAddr;
  Radio.TXdataBuffer[1] = FHSSgetCurrIndex();
  Radio.TXdataBuffer[2] = NonceTX;
  Radio.TXdataBuffer[3] = ((ExpressLRS_currAirRate_Modparams->index & 0b111) << 5) + ((ExpressLRS_currAirRate_Modparams->TLMinterval & 0b111) << 2);
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

#ifdef DYNAMIC_POWER_CHANNEL
  if (crsf.ChannelDataIn[DYNAMIC_POWER_CHANNEL] < 250)
  {
    POWERMGNT.setPower(PWR_10mW);
  }
  
  if (crsf.ChannelDataIn[DYNAMIC_POWER_CHANNEL] >= 250 && crsf.ChannelDataIn[DYNAMIC_POWER_CHANNEL] < 500)
  {
    POWERMGNT.setPower(PWR_25mW);
  }

  if (crsf.ChannelDataIn[DYNAMIC_POWER_CHANNEL] >= 500 && crsf.ChannelDataIn[DYNAMIC_POWER_CHANNEL] < 750)
  {
    POWERMGNT.setPower(PWR_50mW);
  }

  if (crsf.ChannelDataIn[DYNAMIC_POWER_CHANNEL] >= 750 && crsf.ChannelDataIn[DYNAMIC_POWER_CHANNEL] < 1000)
  {
    POWERMGNT.setPower(PWR_100mW);
  }

  if (crsf.ChannelDataIn[DYNAMIC_POWER_CHANNEL] >= 1000 && crsf.ChannelDataIn[DYNAMIC_POWER_CHANNEL] < 1250)
  {
    POWERMGNT.setPower(PWR_250mW);
  }

  if (crsf.ChannelDataIn[DYNAMIC_POWER_CHANNEL] >= 1250 && crsf.ChannelDataIn[DYNAMIC_POWER_CHANNEL] < 1500)
  {
    POWERMGNT.setPower(PWR_500mW);
  }

  if (crsf.ChannelDataIn[DYNAMIC_POWER_CHANNEL] >= 1500 && crsf.ChannelDataIn[DYNAMIC_POWER_CHANNEL] < 1750)
  {
    POWERMGNT.setPower(PWR_1000mW);
  }

  if (crsf.ChannelDataIn[DYNAMIC_POWER_CHANNEL] >= 1750)
  {
    POWERMGNT.setPower(PWR_2000mW);
  }
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
  expresslrs_mod_settings_s *const ModParams = get_elrs_airRateConfig(index);
  expresslrs_rf_pref_params_s *const RFperf = get_elrs_RFperfParams(index);

  Radio.Config(ModParams->bw, ModParams->sf, ModParams->cr, GetInitialFreq(), ModParams->PreambleLen);
  hwTimer.updateInterval(ModParams->interval);

  ExpressLRS_currAirRate_Modparams = ModParams;
  ExpressLRS_currAirRate_RFperfParams = RFperf;

  crsf.RequestedRCpacketInterval = ModParams->interval;
  isRXconnected = false;

  if (UpdateRFparamReq)
  {
    UpdateRFparamReq = false;
  }

  #ifdef PLATFORM_ESP32
  updateLEDs(isRXconnected, ExpressLRS_currAirRate_Modparams->TLMinterval);
  #endif
}

uint8_t ICACHE_RAM_ATTR decTLMrate()
{
  Serial.println("dec TLM");
  uint8_t currTLMinterval = (uint8_t)ExpressLRS_currAirRate_Modparams->TLMinterval;

  if (currTLMinterval < (uint8_t)TLM_RATIO_1_2)
  {
    ExpressLRS_currAirRate_Modparams->TLMinterval = (expresslrs_tlm_ratio_e)(currTLMinterval + 1);
    Serial.println(currTLMinterval);
  }
  return (uint8_t)ExpressLRS_currAirRate_Modparams->TLMinterval;
}

uint8_t ICACHE_RAM_ATTR incTLMrate()
{
  Serial.println("inc TLM");
  uint8_t currTLMinterval = (uint8_t)ExpressLRS_currAirRate_Modparams->TLMinterval;

  if (currTLMinterval > (uint8_t)TLM_RATIO_NO_TLM)
  {
    ExpressLRS_currAirRate_Modparams->TLMinterval = (expresslrs_tlm_ratio_e)(currTLMinterval - 1);
  }
  return (uint8_t)ExpressLRS_currAirRate_Modparams->TLMinterval;
}

void ICACHE_RAM_ATTR decRFLinkRate()
{
  Serial.println("dec RFrate");
  SetRFLinkRate(ExpressLRS_currAirRate_Modparams->index + 1);
}

void ICACHE_RAM_ATTR incRFLinkRate()
{
  Serial.println("inc RFrate");
  SetRFLinkRate(ExpressLRS_currAirRate_Modparams->index - 1);
}

void ICACHE_RAM_ATTR HandleFHSS()
{
  uint8_t modresult = (NonceTX) % ExpressLRS_currAirRate_Modparams->FHSShopInterval;

  if (modresult == 0) // if it time to hop, do so.
  {
    Radio.SetFrequency(FHSSgetNextFreq());
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
    RadioIsIdle = false;
    WaitRXresponse = true;
  }
  else
  {
    RadioIsIdle = true;
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
        return;
      }
      else
      {
        NonceTX++;
      }
    }
  }

  uint32_t SyncInterval;

  if (isRXconnected)
  {
    SyncInterval = ExpressLRS_currAirRate_RFperfParams->SyncPktIntervalConnected;
  }
  else
  {
    SyncInterval = ExpressLRS_currAirRate_RFperfParams->SyncPktIntervalDisconnected;
  }

  if ((millis() > (SyncPacketLastSent + SyncInterval)) && (Radio.currFreq == GetInitialFreq()) && ((NonceTX) % ExpressLRS_currAirRate_Modparams->FHSShopInterval == 1)) // sync just after we changed freqs (helps with hwTimer.init() being in sync from the get go)
  {

    GenerateSyncPacketData();
    SyncPacketLastSent = millis();
    ChangeAirRateSentUpdate = true;
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
    }
    else
    {
#if defined HYBRID_SWITCHES_8
      GenerateChannelDataHybridSwitch8(Radio.TXdataBuffer, &crsf, DeviceAddr);
#elif defined SEQ_SWITCHES
      GenerateChannelDataSeqSwitch(Radio.TXdataBuffer, &crsf, DeviceAddr);
#else
      Generate4ChannelData_11bit();
#endif
    }
  }

  ///// Next, Calculate the CRC and put it into the buffer /////
  uint8_t crc = CalcCRC(Radio.TXdataBuffer, 7) + CRCCaesarCipher;
  Radio.TXdataBuffer[7] = crc;
  Radio.TXnb(Radio.TXdataBuffer, 8);

  if (ChangeAirRateRequested)
  {
    ChangeAirRateSentUpdate = true;
  }
}

void ICACHE_RAM_ATTR ParamUpdateReq()
{
  UpdateParamReq = true;

  if (crsf.ParameterUpdateData[0] == 1)
  {
    UpdateRFparamReq = true;
  }
}

void HandleUpdateParameter()
{

  if (UpdateParamReq == false || RadioIsIdle == false)
  {
    return;
  }

  switch (crsf.ParameterUpdateData[0])
  {
  case 0: // send all params
    Serial.println("send all lua params");
    break;

  case 1:
  Serial.println("Link rate");
    if (crsf.ParameterUpdateData[1] == 0)
    {
      decRFLinkRate();
    }
    else if (crsf.ParameterUpdateData[1] == 1)
    {
      incRFLinkRate();
    }
    Serial.println(ExpressLRS_currAirRate_Modparams->enum_rate);
    break;

  case 2:

    if (crsf.ParameterUpdateData[1] == 0)
    {
      decTLMrate();
    }
    else if (crsf.ParameterUpdateData[1] == 1)
    {
      incTLMrate();
    }

    break;

  case 3:

    if (crsf.ParameterUpdateData[1] == 0)
    {
      Serial.println("Decrease RF power");
      POWERMGNT.decPower();
    }
    else if (crsf.ParameterUpdateData[1] == 1)
    {
      Serial.println("Increase RF power");
      POWERMGNT.incPower();
    }

    break;

  case 4:

    break;

  case 0xFF:
    if (crsf.ParameterUpdateData[1] == 1)
    {
      Serial.println("Binding Requested!");
      uint8_t luaBindingRequestedPacket[] = {(uint8_t)0xFF, (uint8_t)0x01, (uint8_t)0x00, (uint8_t)0x00};
      crsf.sendLUAresponse(luaBindingRequestedPacket);

      //crsf.sendLUAresponse((uint8_t)0xFF, (uint8_t)0x00, (uint8_t)0x00, (uint8_t)0x00); // send this to confirm binding is done
    }
    break;

  default:
    break;
  }

  UpdateParamReq = false;
  //Serial.println("Power");
  //Serial.println(POWERMGNT.currPower());
  uint8_t luaDataPacket[] = {ExpressLRS_currAirRate_Modparams->enum_rate + 3, ExpressLRS_currAirRate_Modparams->TLMinterval + 1, POWERMGNT.currPower() + 2, 4};
  crsf.sendLUAresponse(luaDataPacket);
  
  uint8_t luaCommitPacket[] = {(uint8_t)0xFE, thisCommit[0], thisCommit[1], thisCommit[2]};
  crsf.sendLUAresponse(luaCommitPacket);
  
  uint8_t luaCommitOtherHalfPacket[] = {(uint8_t)0xFD, thisCommit[3], thisCommit[4], thisCommit[5]};
  crsf.sendLUAresponse(luaCommitOtherHalfPacket);
}

void ICACHE_RAM_ATTR RXdoneISR()
{
  ProcessTLMpacket();
}

void ICACHE_RAM_ATTR TXdoneISR()
{
  NonceTX++; // must be done before callback
  RadioIsIdle = true;
  HandleFHSS();
  HandleTLM();
}

void setup()
{
#ifdef PLATFORM_ESP32
  Serial.begin(115200);
  #ifdef USE_UART2
    Serial2.begin(400000);
  #endif
#endif

#if defined(TARGET_R9M_TX) || defined(TARGET_R9M_LITE_TX)

    pinMode(GPIO_PIN_LED_GREEN, OUTPUT);
    pinMode(GPIO_PIN_LED_RED, OUTPUT);
    digitalWrite(GPIO_PIN_LED_GREEN, HIGH);

#ifdef USE_ESP8266_BACKPACK
    HardwareSerial(USART1);
    Serial.begin(460800);
#else
    HardwareSerial(USART2);
    Serial.setTx(PA2);
    Serial.setRx(PA3);
    Serial.begin(400000);
#endif

#if defined(TARGET_R9M_TX)
    // Annoying startup beeps
#ifndef JUST_BEEP_ONCE
  pinMode(GPIO_PIN_BUZZER, OUTPUT);
  const int beepFreq[] = {659, 659, 659, 523, 659, 783, 392};
  const int beepDurations[] = {150, 300, 300, 100, 300, 550, 575};

  for (int i = 0; i < 7; i++)
  {
    tone(GPIO_PIN_BUZZER, beepFreq[i], beepDurations[i]);
    delay(beepDurations[i]);
    noTone(GPIO_PIN_BUZZER);
  }
  #else
  tone(GPIO_PIN_BUZZER, 400, 200);
  delay(200);
  tone(GPIO_PIN_BUZZER, 480, 200);
  #endif
  button.init(GPIO_PIN_BUTTON, true); // r9 tx appears to be active high
  R9DAC.init();
#endif

#endif

#ifdef PLATFORM_ESP32
  //WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector needed for debug, shouldn't need to be actually used in practise.
  strip.Begin();
  // Get base mac address
  esp_read_mac(baseMac, ESP_MAC_WIFI_STA);
  // Print base mac address
  // This should be copied to common.h and is used to generate a unique hop sequence, DeviceAddr, and CRC.
  // UID[0..2] are OUI (organisationally unique identifier) and are not ESP32 unique.  Do not use!
  Serial.println("");
  Serial.println("Copy the below line into common.h.");
  Serial.print("uint8_t UID[6] = {");
  Serial.print(baseMac[0]);
  Serial.print(", ");
  Serial.print(baseMac[1]);
  Serial.print(", ");
  Serial.print(baseMac[2]);
  Serial.print(", ");
  Serial.print(baseMac[3]);
  Serial.print(", ");
  Serial.print(baseMac[4]);
  Serial.print(", ");
  Serial.print(baseMac[5]);
  Serial.println("};");
  Serial.println("");
#endif

  FHSSrandomiseFHSSsequence();

#if defined Regulatory_Domain_AU_915 || defined Regulatory_Domain_EU_868 || defined Regulatory_Domain_FCC_915
#ifdef Regulatory_Domain_EU_868
  Serial.println("Setting 868MHz Mode");
#else
  Serial.println("Setting 915MHz Mode");
#endif
#elif defined Regulatory_Domain_AU_433 || defined Regulatory_Domain_EU_433
  Serial.println("Setting 433MHz Mode");
#endif

  Radio.RXdoneCallback = &RXdoneISR;
  Radio.TXdoneCallback = &TXdoneISR;

#ifndef One_Bit_Switches
  crsf.RCdataCallback1 = &CheckChannels5to8Change;
#endif
  crsf.connected = &hwTimer.resume; // it will auto init when it detects UART connection
  crsf.disconnected = &hwTimer.stop;
  crsf.RecvParameterUpdate = &ParamUpdateReq;
  hwTimer.callbackTock = &TimerCallbackISR;

  Serial.println("ExpressLRS TX Module Booted...");

  POWERMGNT.init();
  Radio.currFreq = GetInitialFreq(); //set frequency first or an error will occur!!!
  Radio.Begin();
  #if !(defined(TARGET_TX_ESP32_E28_SX1280_V1) || defined(TARGET_TX_ESP32_SX1280_V1) || defined(TARGET_RX_ESP8266_SX1280_V1) || defined(Regulatory_Domain_ISM_2400))
  Radio.SetSyncWord(UID[3]);
  #endif 
  POWERMGNT.setDefaultPower();

  SetRFLinkRate(RATE_DEFAULT); // fastest rate by default
  crsf.Begin();
  hwTimer.init();
  hwTimer.stop(); //comment to automatically start the RX timer and leave it running
}

void loop()
{

  while (UpdateParamReq)
  {
    HandleUpdateParameter();
  }

  #ifdef FEATURE_OPENTX_SYNC
  // Serial.println(crsf.OpenTXsyncOffset);
  #endif

  if (millis() > (RX_CONNECTION_LOST_TIMEOUT + LastTLMpacketRecvMillis))
  {
    isRXconnected = false;
    #if defined(TARGET_R9M_TX) || defined(TARGET_R9M_LITE_TX)
    digitalWrite(GPIO_PIN_LED_RED, LOW);
    #endif
  }
  else
  {
    isRXconnected = true;
    #if defined(TARGET_R9M_TX) || defined(TARGET_R9M_LITE_TX)
    digitalWrite(GPIO_PIN_LED_RED, HIGH);
    #endif
  }

  // float targetFrameRate = (ExpressLRS_currAirRate_Modparams->rate * (1.0 / TLMratioEnumToValue(ExpressLRS_currAirRate_Modparams->TLMinterval)));
  // PacketRateLastChecked = millis();
  // PacketRate = (float)packetCounteRX_TX / (float)(PACKET_RATE_INTERVAL);
  // linkQuality = int((((float)PacketRate / (float)targetFrameRate) * 100000.0));

  if (linkQuality > 99)
  {
    linkQuality = 99;
  }
  packetCounteRX_TX = 0;

#if defined(TARGET_R9M_TX) || defined(TARGET_R9M_LITE_TX)
  crsf.STM32handleUARTin();
  #ifdef FEATURE_OPENTX_SYNC
  crsf.sendSyncPacketToTX();
  #endif
  crsf.UARTwdt();
  #ifdef TARGET_R9M_TX
  button.handle();
  #endif
#endif

#ifdef PLATFORM_ESP32
  if (Serial2.available())
  {
    uint8_t c = Serial2.read();
#else
  if (Serial.available())
  {
    uint8_t c = Serial.read();
#endif

    if (msp.processReceivedByte(c))
    {
      // Finished processing a complete packet
      ProcessMSPPacket(msp.getReceivedPacket());
      msp.markPacketReceived();
    }
  }
}

void ICACHE_RAM_ATTR TimerCallbackISR()
{
  if (!UpdateRFparamReq)
  {
    RadioIsIdle = false;
    SendRCdataToRF();
  }
  else
  {
    NonceTX++;
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
    SetRFLinkRate(RATE_200HZ);
    break;
  case RATE_100HZ:
    SetRFLinkRate(RATE_100HZ);
    break;
  case RATE_50HZ:
    SetRFLinkRate(RATE_50HZ);
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
  uint8_t tlmRate = packet->readByte();
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
