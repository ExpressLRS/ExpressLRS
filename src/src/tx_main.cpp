#include <Arduino.h>
#include "FIFO.h"
#include "utils.h"
#include "common.h"
#include "LoRaRadioLib.h"
#include "CRSF.h"
#include "FHSS.h"
#include "LED.h"
// #include "debug.h"
#include "targets.h"

#ifdef TARGET_EXPRESSLRS_PCB_TX_V3
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#endif

//#include "HardwareSerial.h"
//#include "HardwareTimer.h"

String DebugOutput;

/// define some libs to use ///
SX127xDriver Radio;
CRSF crsf;

#ifndef PLATFORM_ESP32
// Hardware timer func defs
void InitHarwareTimer();
void StopHWtimer();
void HWtimerSetCallback(void (*CallbackFunc)(void));
void HWtimerSetCallback90(void (*CallbackFunc)(void));
void HWtimerUpdateInterval(uint32_t Interval);
uint32_t ICACHE_RAM_ATTR HWtimerGetlastCallbackMicros();
uint32_t ICACHE_RAM_ATTR HWtimerGetlastCallbackMicros90();
void ICACHE_RAM_ATTR HWtimerPhaseShift(int16_t Offset);
uint32_t ICACHE_RAM_ATTR HWtimerGetIntervalMicros();

void TimerExpired();
#endif

//// Switch Data Handling ///////
uint8_t SwitchPacketsCounter = 0;             //not used for the moment
uint32_t SwitchPacketSendInterval = 200;      //not used, delete when able to
uint32_t SyncPacketSendIntervalRXlost = 250;  //how often to send the switch data packet (ms) when there is no response from RX
uint32_t SyncPacketSendIntervalRXconn = 1500; //how often to send the switch data packet (ms) when there we have a connection
uint32_t SwitchPacketLastSent = 0;            //time in ms when the last switch data packet was sent

////////////SYNC PACKET/////////
uint32_t SyncPacketLastSent = 0;

uint32_t LastTLMpacketRecvMillis = 0;
uint32_t RXconnectionLostTimeout = 1500; //After 1500ms of no TLM response consider that slave has lost connection
bool isRXconnected = false;
int packetCounteRX_TX = 0;
uint32_t PacketRateLastChecked = 0;
uint32_t PacketRateInterval = 500;
float PacketRate = 0.0;
uint8_t linkQuality = 0;

/// Variables for Sync Behaviour ////
uint32_t RFmodeLastCycled = 0;
uint32_t RFmodeCycleInterval = 1000;
uint32_t SyncPacketAddtionalTime = 1500; //After we have a tentative sync we wait this long in addtion before jumping to different RF mode again.
///////////////////////////////////////

bool UpdateParamReq = false;

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

void ICACHE_RAM_ATTR IncreasePower();
void ICACHE_RAM_ATTR DecreasePower();

uint8_t baseMac[6];

void ICACHE_RAM_ATTR ProcessTLMpacket()
{

  uint8_t calculatedCRC = CalcCRC(Radio.RXdataBuffer, 7) + CRCCaesarCipher;
  uint8_t inCRC = Radio.RXdataBuffer[7];
  uint8_t type = Radio.RXdataBuffer[0] & 0b11;
  uint8_t packetAddr = (Radio.RXdataBuffer[0] & 0b11111100) >> 2;
  uint8_t TLMheader = Radio.RXdataBuffer[1];

  //Serial.println("TLMpacket0");

  if (packetAddr == DeviceAddr)
  {
    if ((inCRC == calculatedCRC))
    {
      packetCounteRX_TX++;
      if (type == 0b11) //tlmpacket
      {
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
          crsf.LinkStatistics.rf_Mode = ExpressLRS_currAirRate.enum_rate;

          crsf.TLMbattSensor.voltage = (Radio.RXdataBuffer[3] << 8) + Radio.RXdataBuffer[6];

          crsf.sendLinkStatisticsToTX();
          crsf.sendLinkBattSensorToTX();
        }
      }
      else
      {
        Serial.println("TLM type error");
        Serial.println(type);
      }
    }
    else
    {
      Serial.println("TLM crc error");
    }
  }
  else
  {
    Serial.println("TLM dev addr");
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
  PacketHeaderAddr = (DeviceAddr << 2) + 0b10;
  Radio.TXdataBuffer[0] = PacketHeaderAddr;
  Radio.TXdataBuffer[1] = FHSSgetCurrIndex();
  Radio.TXdataBuffer[2] = Radio.NonceTX;
  Radio.TXdataBuffer[3] = 0;
  Radio.TXdataBuffer[4] = TxBaseMac[3];
  Radio.TXdataBuffer[5] = TxBaseMac[4];
  Radio.TXdataBuffer[6] = TxBaseMac[5];
}

void ICACHE_RAM_ATTR Generate4ChannelData_10bit()
{
  uint8_t PacketHeaderAddr;
  PacketHeaderAddr = (DeviceAddr << 2) + 0b00;
  Radio.TXdataBuffer[0] = PacketHeaderAddr;
  Radio.TXdataBuffer[1] = ((CRSF_to_UINT10(crsf.ChannelDataIn[0]) & 0b1111111100) >> 2);
  Radio.TXdataBuffer[2] = ((CRSF_to_UINT10(crsf.ChannelDataIn[1]) & 0b1111111100) >> 2);
  Radio.TXdataBuffer[3] = ((CRSF_to_UINT10(crsf.ChannelDataIn[2]) & 0b1111111100) >> 2);
  Radio.TXdataBuffer[4] = ((CRSF_to_UINT10(crsf.ChannelDataIn[3]) & 0b1111111100) >> 2);
  Radio.TXdataBuffer[5] = ((CRSF_to_UINT10(crsf.ChannelDataIn[0]) & 0b0000000011) << 6) + ((CRSF_to_UINT10(crsf.ChannelDataIn[1]) & 0b0000000011) << 4) +
                          ((CRSF_to_UINT10(crsf.ChannelDataIn[2]) & 0b0000000011) << 2) + ((CRSF_to_UINT10(crsf.ChannelDataIn[3]) & 0b0000000011) << 0);
}

void ICACHE_RAM_ATTR Generate4ChannelData_11bit()
{
  uint8_t PacketHeaderAddr;
  PacketHeaderAddr = (DeviceAddr << 2) + 0b00;
  Radio.TXdataBuffer[0] = PacketHeaderAddr;
  Radio.TXdataBuffer[1] = ((crsf.ChannelDataIn[0]) >> 3);
  Radio.TXdataBuffer[2] = ((crsf.ChannelDataIn[1]) >> 3);
  Radio.TXdataBuffer[3] = ((crsf.ChannelDataIn[2]) >> 3);
  Radio.TXdataBuffer[4] = ((crsf.ChannelDataIn[3]) >> 3);
  Radio.TXdataBuffer[5] = ((crsf.ChannelDataIn[0] & 0b00000111) << 5) + ((crsf.ChannelDataIn[1] & 0b111) << 2) + ((crsf.ChannelDataIn[2] & 0b110) >> 1);
  Radio.TXdataBuffer[6] = ((crsf.ChannelDataIn[2] & 0b001) << 7) + ((crsf.ChannelDataIn[3] & 0b111) << 4); // 4 bits left over for something else?
#ifdef One_Bit_Switches
  Radio.TXdataBuffer[6] += CRSF_to_BIT(crsf.ChannelDataIn[4]) << 3;
  Radio.TXdataBuffer[6] += CRSF_to_BIT(crsf.ChannelDataIn[5]) << 2;
  Radio.TXdataBuffer[6] += CRSF_to_BIT(crsf.ChannelDataIn[6]) << 1;
  Radio.TXdataBuffer[6] += CRSF_to_BIT(crsf.ChannelDataIn[7]) << 0;
#endif
}

void ICACHE_RAM_ATTR GenerateSwitchChannelData()
{
  uint8_t PacketHeaderAddr;
  PacketHeaderAddr = (DeviceAddr << 2) + 0b01;
  Radio.TXdataBuffer[0] = PacketHeaderAddr;
  Radio.TXdataBuffer[1] = ((CRSF_to_UINT10(crsf.ChannelDataIn[4]) & 0b1110000000) >> 2) + ((CRSF_to_UINT10(crsf.ChannelDataIn[5]) & 0b1110000000) >> 5) + ((CRSF_to_UINT10(crsf.ChannelDataIn[6]) & 0b1100000000) >> 8);
  Radio.TXdataBuffer[2] = (CRSF_to_UINT10(crsf.ChannelDataIn[6]) & 0b0010000000) + ((CRSF_to_UINT10(crsf.ChannelDataIn[7]) & 0b1110000000) >> 3);
  Radio.TXdataBuffer[3] = Radio.TXdataBuffer[1];
  Radio.TXdataBuffer[4] = Radio.TXdataBuffer[2];
  Radio.TXdataBuffer[5] = Radio.NonceTX;
  Radio.TXdataBuffer[6] = FHSSgetCurrIndex();
}

void SetRFLinkRate(expresslrs_mod_settings_s mode) // Set speed of RF link (hz)
{
#ifdef PLATFORM_ESP32
  Radio.TimerInterval = mode.interval;
  Radio.UpdateTimerInterval();
#else
  HWtimerUpdateInterval(mode.interval); // TODO: Make sure this is equiv to above commented lines
#endif
  Radio.Config(mode.bw, mode.sf, mode.cr, Radio.currFreq, Radio._syncWord);
  Radio.SetPreambleLength(mode.PreambleLen);
  ExpressLRS_prevAirRate = ExpressLRS_currAirRate;
  ExpressLRS_currAirRate = mode;
  crsf.RequestedRCpacketInterval = ExpressLRS_currAirRate.interval;
  DebugOutput += String(mode.rate) + "Hz";
  isRXconnected = false;
}

void ICACHE_RAM_ATTR HandleFHSS()
{
  uint8_t modresult = (Radio.NonceTX) % ExpressLRS_currAirRate.FHSShopInterval;

  if (modresult == 0) // if it time to hop, do so.
  {
    Radio.SetFrequency(FHSSgetNextFreq());
  }
}

void ICACHE_RAM_ATTR HandleTLM()
{
  if (ExpressLRS_currAirRate.TLMinterval > 0)
  {
    uint8_t modresult = (Radio.NonceTX) % TLMratioEnumToValue(ExpressLRS_currAirRate.TLMinterval);

    if (modresult == 0) // wait for tlm response because it's time
    {
      Radio.RXnb();
      WaitRXresponse = true;
    }
  }
}

void ICACHE_RAM_ATTR SendRCdataToRF()
{

#ifdef FEATURE_OPENTX_SYNC
  crsf.JustSentRFpacket(); // tells the crsf that we want to send data now - this allows opentx packet syncing
#endif

  /////// This Part Handles the Telemetry Response ///////
  if (ExpressLRS_currAirRate.TLMinterval > 0)
  {
    uint8_t modresult = (Radio.NonceTX) % TLMratioEnumToValue(ExpressLRS_currAirRate.TLMinterval);

    if (modresult == 0)
    { // wait for tlm response
      if (WaitRXresponse == true)
      {
        WaitRXresponse = false;
        return;
      }
      else
      {
        Radio.NonceTX++;
      }
    }
  }

  uint32_t SyncInterval;

  if (isRXconnected)
  {
    SyncInterval = SyncPacketSendIntervalRXconn;
  }
  else
  {
    SyncInterval = SyncPacketSendIntervalRXlost;
  }

  //if (((millis() > (SyncPacketLastSent + SyncInterval)) && (Radio.currFreq == GetInitialFreq())) || ChangeAirRateRequested) //only send sync when its time and only on channel 0;
  if ((millis() > (SyncPacketLastSent + SyncInterval)) && (Radio.currFreq == GetInitialFreq()))
  {

    GenerateSyncPacketData();
    SyncPacketLastSent = millis();
    ChangeAirRateSentUpdate = true;
    //Serial.println("sync");
    //Serial.println(Radio.currFreq);
  }
  else
  {
    if ((millis() > (SwitchPacketSendInterval + SwitchPacketLastSent)) || Channels5to8Changed)
    {
      Channels5to8Changed = false;
      GenerateSwitchChannelData();
      SwitchPacketLastSent = millis();
    }
    else // else we just have regular channel data which we send as 8 + 2 bits
    {
      Generate4ChannelData_11bit();
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
}

void ICACHE_RAM_ATTR HandleUpdateParameter()
{

  if (UpdateParamReq == true)
  {
    switch (crsf.ParameterUpdateData[0])
    {
    case 1:
      if (ExpressLRS_currAirRate.enum_rate != (expresslrs_RFrates_e)crsf.ParameterUpdateData[1])
      {
        SetRFLinkRate(ExpressLRS_AirRateConfig[crsf.ParameterUpdateData[1]]);
      }
      break;

    case 2:

      break;
    case 3:

      switch (crsf.ParameterUpdateData[1])
      {
      case 0:
        Radio.maxPWR = 0b1111;
        Radio.SetOutputPower(0b1111); // 500 mW
        Serial.println("Setpower 500 mW");
        break;

      case 1:
        Radio.maxPWR = 0b1000;
        Radio.SetOutputPower(0b1111);
        Serial.println("Setpower 200 mW");
        break;

      case 2:
        Radio.maxPWR = 0b1000;
        Radio.SetOutputPower(0b1000);
        Serial.println("Setpower 100 mW");
        break;

      case 3:
        Radio.maxPWR = 0b0101;
        Radio.SetOutputPower(0b0101);
        Serial.println("Setpower 50 mW");
        break;

      case 4:
        Radio.maxPWR = 0b0010;
        Radio.SetOutputPower(0b0010);
        Serial.println("Setpower 25 mW");
        break;

      case 5:
        Radio.maxPWR = 0b0000;
        Radio.SetOutputPower(0b0000);
        Serial.println("Setpower Pit");
        break;

      default:
        break;
      }

      break;
    case 4:

      break;

    default:
      break;
    }
  }

  UpdateParamReq = false;
}

// void ICACHE_RAM_ATTR IncreasePower()
// {
//   if (Radio.currPWR < Radio.maxPWR)
//   {
//     Radio.SetOutputPower(Radio.currPWR + 1);
//   }
// }

// void ICACHE_RAM_ATTR DecreasePower()
// {
//   if (Radio.currPWR > 0)
//   {
//     Radio.SetOutputPower(Radio.currPWR - 1);
//   }
// }

void DetectOtherRadios()
{
  Radio.SetFrequency(GetInitialFreq());
  //Radio.RXsingle();

  // if (Radio.RXsingle(RXdata, 7, 2 * (RF_RATE_50HZ.interval / 1000)) == ERR_NONE)
  // {
  //   Serial.println("got fastsync resp 1");
  //   break;
  // }
}

void setup()
{
#ifdef LEGACY_HARDWARE
  pinMode(4, INPUT_PULLDOWN);
#endif
  pinMode(36, INPUT_PULLUP);

#ifdef PLATFORM_ESP32
  Serial.begin(115200);

#endif

#ifdef TARGET_R9M_TX
  HardwareSerial(USART2);
  Serial.setTx(GPIO_PIN_DEBUG_TX);
  Serial.setRx(GPIO_PIN_DEBUG_RX);
  Serial.begin(115200);
  pinMode(GPIO_PIN_BUZZER, OUTPUT);
  for (int i = 0; i < 500; i++)
  {
    digitalWrite(GPIO_PIN_BUZZER, HIGH);
    delayMicroseconds(200);
    digitalWrite(GPIO_PIN_BUZZER, LOW);
    delayMicroseconds(200);
  }
#endif

  Serial.println("ExpressLRS TX Module Booted...");

#ifdef TARGET_EXPRESSLRS_PCB_TX_V3
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector

  strip.Begin();

  // Get base mac address
  esp_read_mac(baseMac, ESP_MAC_WIFI_STA);

  // Print base mac address
  // This should be copied to common.h and is used to generate a unique hop sequence, DeviceAddr, and CRC.
  // TxBaseMac[0..2] are OUI (organisationally unique identifier) and are not ESP32 unique.  Do not use!
  Serial.println("");
  Serial.println("Copy the below line into common.h.");
  Serial.print("uint8_t TxBaseMac[6] = {");
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

#ifdef Regulatory_Domain_AU_915
  Serial.println("Setting 915MHz Mode");
  Radio.RFmodule = RFMOD_SX1276; //define radio module here
#ifdef TARGET_100mW_MODULE
  Radio.SetOutputPower(0b1111); // 20dbm = 100mW
#else                           // Below output power settings are for 1W modules
  // Radio.SetOutputPower(0b0000); // 15dbm = 32mW
  // Radio.SetOutputPower(0b0001); // 18dbm = 40mW
  // Radio.SetOutputPower(0b0101); // 20dbm = 100mW
  Radio.SetOutputPower(0b1000); // 23dbm = 200mW
                                // Radio.SetOutputPower(0b1100); // 27dbm = 500mW
                                // Radio.SetOutputPower(0b1111); // 30dbm = 1000mW
#endif
#elif defined Regulatory_Domain_AU_433
  Serial.println("Setting 433MHz Mode");
  Radio.RFmodule = RFMOD_SX1278; //define radio module here
  Radio.SetOutputPower(0b1111);
#endif

  Radio.SetOutputPower(0b0000);

  Radio.SetFrequency(GetInitialFreq()); //set frequency first or an error will occur!!!

  Radio.RXdoneCallback1 = &ProcessTLMpacket;

  Radio.TXdoneCallback1 = &HandleFHSS;
  Radio.TXdoneCallback2 = &HandleTLM;
  Radio.TXdoneCallback3 = &HandleUpdateParameter;
  //Radio.TXdoneCallback4 = &NULL;

  Radio.TimerDoneCallback = &SendRCdataToRF;

#ifndef One_Bit_Switches
  crsf.RCdataCallback1 = &CheckChannels5to8Change;
#endif

#ifdef PLATFORM_STM32
  crsf.connected = &InitHarwareTimer;
  crsf.disconnected = &StopHWtimer;
  crsf.RecvParameterUpdate = &ParamUpdateReq;
  HWtimerSetCallback(TimerExpired);
  InitHarwareTimer();
#endif

  Radio.Begin();
  crsf.Begin();

  SetRFLinkRate(RF_RATE_100HZ);
}

void loop()
{

  //delay(10);

  // if (digitalRead(4) == 0)
  // {
  //   Serial.println("Switch Pressed!");
  // }

  // if (digitalRead(36) == 0)
  // {
  //   Serial.println("Switch Pressed!");
  // }

#ifdef FEATURE_OPENTX_SYNC
  //Serial.println(crsf.OpenTXsyncOffset);
#endif

  updateLEDs(isRXconnected, ExpressLRS_currAirRate.TLMinterval);

  if (millis() > (RXconnectionLostTimeout + LastTLMpacketRecvMillis))
  {
    isRXconnected = false;
  }
  else
  {
    isRXconnected = true;
  }

  //if (millis() > (PacketRateLastChecked + PacketRateInterval)//just some debug data
  //{
  // if (isRXconnected)
  // {
  //   if ((Radio.RXdataBuffer[2] < 30 || Radio.RXdataBuffer[4] < 10))
  //   {
  //     IncreasePower();
  //   }
  //   if (Radio.RXdataBuffer[2] > 60 || Radio.RXdataBuffer[4] > 40)
  //   {
  //     DecreasePower();
  //   }
  //   crsf.sendLinkStatisticsToTX();
  // }

  float targetFrameRate = (ExpressLRS_currAirRate.rate * (1.0 / TLMratioEnumToValue(ExpressLRS_currAirRate.TLMinterval)));
  PacketRateLastChecked = millis();
  PacketRate = (float)packetCounteRX_TX / (float)(PacketRateInterval);
  linkQuality = int((((float)PacketRate / (float)targetFrameRate) * 100000.0));

  if (linkQuality > 99)
  {
    linkQuality = 99;
  }
  packetCounteRX_TX = 0;

#ifdef TARGET_R9M_TX
  crsf.STM32handleUARTin();
  crsf.sendSyncPacketToTX();
  crsf.STM32wdtUART();
#endif
}

void ICACHE_RAM_ATTR TimerExpired()
{
  SendRCdataToRF();
}
