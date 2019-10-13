#include <Arduino.h>
#include "common.h"
#include "LoRaRadioLib.h"
#include "CRSF.h"
#include "FHSS.h"
#include "utils.h"

//#define RFmodule_Size FULL
#define Regulatory_Domain_AU_433 // define frequnecy band of operation

void SetRFLinkRate(expresslrs_mod_settings_s mode);
void BeginFastSync();

void ICACHE_RAM_ATTR nullTask(){};

String DebugOutput;

/// define some libs to use ///
SX127xDriver Radio;
CRSF crsf;

uint8_t SwitchPacketsCounter = 0;              //not used for the moment
uint32_t SwitchPacketSendInterval = 200;       //not used, delete when able to
uint32_t SwitchPacketSendIntervalRXlost = 200; //how often to send the switch data packet (ms) when there is no response from RX
uint32_t SwitchPacketSendIntervalRXconn = 200; //how often to send the switch data packet (ms) when there we have a connection
uint32_t SwitchPacketLastSent = 0;             //time in ms when the last switch data packet was sent

////////////FHSS SYNC PACKET/////////
//only send sync packet when we don't have a slave response for > 1.5sec?
uint32_t SyncPacketLastSent = 0;
uint32_t SyncPacketInterval = 200;

uint8_t TelemetryWaitBuffer[7] = {0};

/////RX -> Link Quality Variables/////
#define TLMpacketRatio 16
#define HopInterval 8

uint32_t LinkSpeedIncreaseDelayFactor = 500; // wait for the connection to be 'good' for this long before increasing the speed.
uint32_t LinkSpeedDecreaseDelayFactor = 200; // this long wait this long for connection to be below threshold before dropping speed

uint32_t LinkSpeedDecreaseFirstMetCondition = 0;
uint32_t LinkSpeedIncreaseFirstMetCondition = 0;

uint8_t LinkSpeedReduceSNR = 20;   //if the SNR (times 10) is lower than this we drop the link speed one level
uint8_t LinkSpeedIncreaseSNR = 60; //if the SNR (times 10) is higher than this we increase the link speed

uint32_t LastTLMpacketRecvMillis = 0;
uint32_t RXconnectionLostTimeout = 1000; //After 1.5 seconds of no TLM response consider that slave has lost connection
bool isRXconnected = false;
int packetCounteRX_TX = 0;
uint32_t PacketRateLastChecked = 0;
uint32_t PacketRateInterval = 500;
float PacketRate = 0.0;
uint8_t linkQuality = 0;
//float targetFrameRate = 3.125;
///////////////////////////////////////

bool Channels5to8Changed = false;
bool blockUpdate = false;
bool ChangeAirRate = false;
bool SentAirRateInfo = false;

bool resync = false;

bool WaitRXresponse = false;

uint8_t DeviceAddr = 0b101010;

void ICACHE_RAM_ATTR ProcessTLMpacket()
{
  uint8_t calculatedCRC = CalcCRC(Radio.RXdataBuffer, 6);
  uint8_t inCRC = Radio.RXdataBuffer[6];
  uint8_t type = Radio.RXdataBuffer[0] & 0b11;
  uint8_t packetAddr = (Radio.RXdataBuffer[0] & 0b11111100) >> 2;
  uint8_t TLMheader = Radio.RXdataBuffer[1];

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
          crsf.LinkStatistics.uplink_RSSI_1 = -Radio.RXdataBuffer[2];
          crsf.LinkStatistics.uplink_RSSI_2 = 0;
          crsf.LinkStatistics.uplink_SNR = Radio.RXdataBuffer[4];
          crsf.LinkStatistics.uplink_Link_quality = Radio.RXdataBuffer[5];

          crsf.LinkStatistics.downlink_SNR = int(Radio.LastPacketSNR * 10);
          crsf.LinkStatistics.downlink_RSSI = 120 + Radio.LastPacketRSSI;
          crsf.LinkStatistics.downlink_Link_quality = linkQuality;

          crsf.sendLinkStatisticsToTX();
        }
      }
    }
  }
}

void ICACHE_RAM_ATTR CheckChannels5to8Change()
{ //check if channels 5 to 8 have new data (switch channels)
  for (int i = 4; i < 8; i++)
  {
    if (crsf.ChannelDataInPrev[i] != crsf.ChannelDataIn[i])
    {
      Channels5to8Changed = true;
      if (i == 7)
      {
        ChangeAirRate = true;
        blockUpdate = true;
      }
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
  Radio.TXdataBuffer[3] = ExpressLRS_currAirRate.enum_rate;
  Radio.TXdataBuffer[4] = 0;
}

void ICACHE_RAM_ATTR Generate4ChannelData()
{
  uint8_t PacketHeaderAddr;
  PacketHeaderAddr = (DeviceAddr << 2) + 0b00;
  Radio.TXdataBuffer[0] = PacketHeaderAddr;
  Radio.TXdataBuffer[1] = ((CRSF_to_UINT11(crsf.ChannelDataIn[0]) & 0b1111111100) >> 2);
  Radio.TXdataBuffer[2] = ((CRSF_to_UINT11(crsf.ChannelDataIn[1]) & 0b1111111100) >> 2);
  Radio.TXdataBuffer[3] = ((CRSF_to_UINT11(crsf.ChannelDataIn[2]) & 0b1111111100) >> 2);
  Radio.TXdataBuffer[4] = ((CRSF_to_UINT11(crsf.ChannelDataIn[3]) & 0b1111111100) >> 2);
  Radio.TXdataBuffer[5] = ((CRSF_to_UINT11(crsf.ChannelDataIn[0]) & 0b0000000011) << 6) + ((CRSF_to_UINT11(crsf.ChannelDataIn[1]) & 0b0000000011) << 4) +
                          ((CRSF_to_UINT11(crsf.ChannelDataIn[2]) & 0b0000000011) << 2) + ((CRSF_to_UINT11(crsf.ChannelDataIn[3]) & 0b0000000011) << 0);
}

void ICACHE_RAM_ATTR GenerateSwitchChannelData()
{
  uint8_t PacketHeaderAddr;
  PacketHeaderAddr = (DeviceAddr << 2) + 0b01;
  Radio.TXdataBuffer[0] = PacketHeaderAddr;
  Radio.TXdataBuffer[1] = ((CRSF_to_UINT11(crsf.ChannelDataIn[4]) & 0b1110000000) >> 2) + ((CRSF_to_UINT11(crsf.ChannelDataIn[5]) & 0b1110000000) >> 5) + ((CRSF_to_UINT11(crsf.ChannelDataIn[6]) & 0b1100000000) >> 8);
  Radio.TXdataBuffer[2] = (CRSF_to_UINT11(crsf.ChannelDataIn[6]) & 0b0010000000) + ((CRSF_to_UINT11(crsf.ChannelDataIn[7]) & 0b1110000000) >> 3);
  Radio.TXdataBuffer[3] = Radio.TXdataBuffer[1];
  Radio.TXdataBuffer[4] = Radio.TXdataBuffer[2];
  Radio.TXdataBuffer[5] = Radio.NonceTX; //we use this free byte in switch packet to send the current nonce and sync the RX to the TX
}

void ICACHE_RAM_ATTR HandleFHSS()
{
  uint8_t modresult = (Radio.NonceTX) % ExpressLRS_currAirRate.FHSShopInterval;

  if (modresult == 0) // if it time to hop, do so.
  {
    Radio.SetFrequency(FHSSgetNextFreq());
  }
}

void ICACHE_RAM_ATTR SendRCdataToRF_NoTLM()
{
  uint32_t SyncInterval;
  if (isRXconnected)
  {
    SyncInterval = SwitchPacketSendIntervalRXconn;
  }
  else
  {
    SyncInterval = SwitchPacketSendIntervalRXlost;
  }

  if (((millis() > (SyncPacketLastSent + SyncInterval)) && (Radio.currFreq == GetInitialFreq())) || ChangeAirRate) //only send sync when its time and only on channel 0;
  //if ((millis() > (SyncPacketLastSent + SyncInterval)))
  //if(FHSSgetCurrFreq() == GetInitialFreq())
  {

    GenerateSyncPacketData();
    SyncPacketLastSent = millis();
    SentAirRateInfo = true;
    Serial.println("sync");
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
      Generate4ChannelData();
    }
  }

  ///// Next, Calculate the CRC and put it into the buffer /////
  uint8_t crc = CalcCRC(Radio.TXdataBuffer, 6);
  Radio.TXdataBuffer[6] = crc;
  ////////////////////////////////////////////////////////////
  Radio.TXnb(Radio.TXdataBuffer, 7);
  blockUpdate = false;
}

void SetRFLinkRate(expresslrs_mod_settings_s mode) // Set speed of RF link (hz)
{
  Radio.TimerInterval = mode.interval;
  Radio.UpdateTimerInterval();
  Radio.Config(mode.bw, mode.sf, mode.cr, Radio.currFreq, Radio._syncWord);
  Radio.SetPreambleLength(mode.PreambleLen);
  ExpressLRS_prevAirRate = ExpressLRS_currAirRate;
  ExpressLRS_currAirRate = mode;
  DebugOutput += String(mode.rate) + "Hz";
}

void BeginFastSync()
{ //sync and get lock to reciever
  Serial.println("Begin fastsync");
  uint8_t RXdata[7];
  Radio.StopTimerTask();
  SetRFLinkRate(RF_RATE_50HZ);
  Radio.SetFrequency(GetInitialFreq());

  GenerateSyncPacketData();
  Radio.TXdataBuffer[3] = ExpressLRS_prevAirRate.enum_rate;
  Radio.TXdataBuffer[4] = 1;
  uint8_t crc = CalcCRC(Radio.TXdataBuffer, 6);
  Radio.TXdataBuffer[6] = crc;

  while (1)
  {
    Radio.SetFrequency(FHSSgetNextFreq());
    Radio.TX((uint8_t *)Radio.TXdataBuffer, 7);
    //vTaskDelay(1);
    if (Radio.RXsingle(RXdata, 7, 2 * (RF_RATE_50HZ.interval / 1000)) == ERR_NONE)
    {
      Serial.println("got fastsync resp 1");
      break;
    }
  }

  uint8_t calculatedCRC = CalcCRC(RXdata, 6);
  uint8_t inCRC = RXdata[6];
  uint8_t type = RXdata[0] & 0b11;
  uint8_t packetAddr = (RXdata[0] & 0b11111100) >> 2;

  if (packetAddr == DeviceAddr)
  {
    if ((inCRC == calculatedCRC))
    {
      if (type == 0b10) //sync packet
      {
        if (RXdata[4] == 2)
        {
          Serial.println("got SYNACK"); // got SYNACK
          LastTLMpacketRecvMillis = millis();
          GenerateSyncPacketData();

          Radio.TXdataBuffer[3] = ExpressLRS_prevAirRate.enum_rate;
          Radio.TXdataBuffer[4] = 3;
          uint8_t crc = CalcCRC(Radio.TXdataBuffer, 6);
          Radio.TXdataBuffer[6] = crc;
          Radio.TX((uint8_t *)Radio.TXdataBuffer, 7);
          SetRFLinkRate(ExpressLRS_prevAirRate);
          Radio.StartTimerTask();
          Serial.println("fastsync done!");
        }
      }
    }
  }
  else
  {
    BeginFastSync();
  }
}

void UpdateAirRate()
{
  if (ChangeAirRate && !blockUpdate) //airrate change has been changed and we also informed the slave
  {
    uint32_t startTime = micros();
    Serial.println("changing RF rate");
    int data = crsf.ChannelDataIn[7];

    if (data >= 0 && data < 743)
    {
      SetRFLinkRate(RF_RATE_200HZ);
    }

    if (data >= 743 && data < 1313)
    {
      SetRFLinkRate(RF_RATE_100HZ);
    }

    if (data >= 1313 && data <= 1811)
    {
      SetRFLinkRate(RF_RATE_50HZ);
    }
    ChangeAirRate = false;
    SentAirRateInfo = false;
    blockUpdate = false;
    resync = true;
    Serial.println(micros() - startTime);
  }
}

void setup()
{
  Serial.begin(115200);
  delay(3000);
  Serial.println("ExpressLRS TX Module Booted...");

#ifdef Regulatory_Domain_AU_915
  Serial.println("Setting 915MHz Mode");
  FHSSsetFreqMode(915);
  Radio.RFmodule = RFMOD_SX1276;        //define radio module here
  Radio.SetFrequency(GetInitialFreq()); //set frequency first or an error will occur!!!

  // Radio.SetOutputPower(0b0000); // 15dbm = 32mW
  // Radio.SetOutputPower(0b0001); // 18dbm = 40mW
  Radio.SetOutputPower(0b0000); // 20dbm = 100mW
                                //Radio.SetOutputPower(0b1000); // 23dbm = 200mW
                                // Radio.SetOutputPower(0b1100); // 27dbm = 500mW
                                // Radio.SetOutputPower(0b1111); // 30dbm = 1000mW
#elif defined Regulatory_Domain_AU_433
  Serial.println("Setting 433MHz Mode");
  FHSSsetFreqMode(433);
  Radio.RFmodule = RFMOD_SX1278;        //define radio module here
  Radio.SetFrequency(GetInitialFreq()); //set frequency first or an error will occur!!!
  Serial.println(FHSSgetNextFreq());
  Radio.SetOutputPower(0b1111);
#endif

  Radio.HighPowerModule = true;
  Radio.TimerInterval = 10000; //in microseconds
  Radio.TXbuffLen = 7;
  Radio.RXbuffLen = 7;

  Radio.RXdoneCallback1 = &ProcessTLMpacket;
  Radio.TXdoneCallback1 = &HandleFHSS;
  Radio.TXdoneCallback2 = &UpdateAirRate;

  Radio.TimerDoneCallback = &SendRCdataToRF_NoTLM;

  crsf.RCdataCallback1 = &CheckChannels5to8Change;
  crsf.connected = &Radio.StartTimerTask;
  crsf.disconnected = &Radio.StopTimerTask;

  Radio.Begin();
  SetRFLinkRate(RF_RATE_200HZ);
  //Radio.StartTimerTask(); // not needed if triggered by CRSF uart detection
  //BeginFastSync();
  crsf.Begin();
}

void loop()
{
  // int min = 9999;

  // if (Radio.HeadRoom < min)
  // {
  //   min = Radio.HeadRoom
  // }
  vTaskDelay(100);

  if (resync && SentAirRateInfo)
  {
    resync = false;
    //BeginFastSync();
  }

  //Serial.println(Radio.HeadRoom);
  //UpdateAirRate();

  // if (crsf.CRSFstate == false)
  // {
  //   Radio.StopTimerTask();
  // }
  // else
  // {
  //   Radio.StartTimerTask();
  // }

  //  for (int i = 0; i<255; i++){
  //    Serial.println(i);
  //delay(100);
  //Serial.println("sending..");
  //Serial.println(Radio.TX(testdata, 5));
  //}
  // Serial.print("SPItime: ");
  // Serial.println(Radio.TXspiTime);
  // Serial.print("TotalTime: ");
  // Serial.println(Radio.TimeOnAir);
  // Serial.print("HeadRoom: ");
  // Serial.println(Radio.HeadRoom);
  //Serial.print("PacketCount(HZ): ");
  //Serial.println(Radio.PacketCount);
  // Radio.PacketCount = 0;
  //SendRCdataToRF();
  //PrintRC();
  // Radio.StartContTX();
  // Serial.println("StartCont");
  // delay(10);
  // Radio.StopContTX();
  // Serial.println("StopContTX");
  // Radio.StartContRX();
  //delay(4);
  //PrintRC();

  //delay(250);
  //Serial.println(Radio.currPWR);

  if (millis() > (RXconnectionLostTimeout + LastTLMpacketRecvMillis))
  {
    isRXconnected = false;
  }

  // if (millis() > (PacketRateLastChecked + PacketRateInterval)) //just some debug data
  // {
  //   float targetFrameRate = (ExpressLRS_currAirRate.rate * (1.0 / ExpressLRS_currAirRate.TLMinterval));
  //   PacketRateLastChecked = millis();
  //   PacketRate = (float)packetCounteRX_TX / (float)(PacketRateInterval);
  //   linkQuality = int((((float)PacketRate / (float)targetFrameRate) * 100000.0));

  //   if (linkQuality > 99)
  //   {
  //     linkQuality = 99;
  //   }
  //   packetCounteRX_TX = 0;
  // }
}
