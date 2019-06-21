#include <Arduino.h>
#include "common.h"
#include "LoRaRadioLib.h"
#include "CRSF.h"
#include "FHSS.h"

uint8_t testdata[7] = {1, 2, 3, 4, 5, 6, 7};
uint32_t TXdoneMicros = 0;

uint8_t SwitchPacketsCounter = 0;        //not used for the moment
uint32_t SwitchPacketSendInterval = 250; //how often to send the switch data packet (ms)
uint32_t SwitchPacketLastSent = 0;       //time in ms when the last switch data packet was sent

////////////FHSS SYNC PACKET/////////
//only send sync packet when we don't have a slave response for > 1.5sec?
uint32_t SyncPacketLastSent = 0;
uint32_t SyncPacketInterval = 200;

/////RX -> Link Quality Variables/////
#define TLMpacketRatio 16
#define HopInterval 8

uint32_t LastTLMpacketRecvMillis = 0;
uint32_t RXconnectionLostTimeout = 1500; //After 1.5 seconds of no TLM response consider that slave has lost connection
bool isRXconnectionLost = true;
int packetCounteRX_TX = 0;
uint32_t PacketRateLastChecked = 0;
uint32_t PacketRateInterval = 1000;
float PacketRate = 0.0;
float linkQuality = 0.0;
float targetFrameRate = 6.25;
///////////////////////////////////////

bool WaitRXresponse = false;

uint8_t DeviceAddr = 0b101010;

SX127xDriver Radio;
CRSF crsf;

void ICACHE_RAM_ATTR ProcessTLMpacket()
{
  Serial.print("-");
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
        isRXconnectionLost = false;
        LastTLMpacketRecvMillis = millis();

        if (TLMheader == CRSF_FRAMETYPE_LINK_STATISTICS)
        {
          crsf.LinkStatistics.uplink_RSSI_1 = Radio.RXdataBuffer[2];
          crsf.LinkStatistics.uplink_RSSI_2 = Radio.RXdataBuffer[3];
          crsf.LinkStatistics.uplink_SNR = Radio.RXdataBuffer[4];
          crsf.LinkStatistics.uplink_Link_quality = Radio.RXdataBuffer[5];

          crsf.LinkStatistics.downlink_SNR = int(Radio.GetLastPacketSNR() * 10);
          crsf.LinkStatistics.downlink_RSSI = 120 + Radio.GetLastPacketRSSI();
          crsf.LinkStatistics.downlink_Link_quality = int(linkQuality);

          crsf.sendLinkStatisticsToTX();
        }
      }
    }
  }
}

void ICACHE_RAM_ATTR PrintRC()
{
  Serial.print(crsf.ChannelDataIn[0]);
  Serial.print(",");
  Serial.print(crsf.ChannelDataIn[1]);
  Serial.print(",");
  Serial.print(crsf.ChannelDataIn[2]);
  Serial.print(",");
  Serial.print(crsf.ChannelDataIn[3]);
  Serial.print(",");
  Serial.println(crsf.ChannelDataIn[4]);
  Serial.print(",");
  Serial.print(crsf.ChannelDataIn[5]);
  Serial.print(",");
  Serial.println(crsf.ChannelDataIn[6]);
  Serial.print(",");
  Serial.println(crsf.ChannelDataIn[7]);
}

bool ICACHE_RAM_ATTR CheckChannels5to8Change()
{ //check if channels 5 to 8 have new data (switch channels)
  bool result = false;
  for (int i = 4; i < 8; i++)
  {
    if (crsf.ChannelDataInPrev[i] != crsf.ChannelDataIn[i])
    {
      result = true;
      break;
    }
  }
  return result;
}

void ICACHE_RAM_ATTR SetupSyncPacketData()
{
  uint8_t PacketHeaderAddr;
  PacketHeaderAddr = 0b10101010;
  Radio.TXdataBuffer[0] = PacketHeaderAddr;
  Radio.TXdataBuffer[1] = FHSSgetCurrIndex();
  Radio.TXdataBuffer[2] = Radio.NonceTX;
  //Radio.TXdataBuffer[3] = crsf.frameInterval;
}

void ICACHE_RAM_ATTR Setup4ChannelData()
{
  uint8_t PacketHeaderAddr;
  PacketHeaderAddr = 0b10101000;
  Radio.TXdataBuffer[0] = PacketHeaderAddr;
  Radio.TXdataBuffer[1] = ((CRSF_to_UINT11(crsf.ChannelDataIn[0]) & 0b1111111100) >> 2);
  Radio.TXdataBuffer[2] = ((CRSF_to_UINT11(crsf.ChannelDataIn[1]) & 0b1111111100) >> 2);
  Radio.TXdataBuffer[3] = ((CRSF_to_UINT11(crsf.ChannelDataIn[2]) & 0b1111111100) >> 2);
  Radio.TXdataBuffer[4] = ((CRSF_to_UINT11(crsf.ChannelDataIn[3]) & 0b1111111100) >> 2);
  Radio.TXdataBuffer[5] = ((CRSF_to_UINT11(crsf.ChannelDataIn[0]) & 0b0000000011) << 6) + ((CRSF_to_UINT11(crsf.ChannelDataIn[1]) & 0b0000000011) << 4) +
                          ((CRSF_to_UINT11(crsf.ChannelDataIn[2]) & 0b0000000011) << 2) + ((CRSF_to_UINT11(crsf.ChannelDataIn[3]) & 0b0000000011) << 0);
}

void ICACHE_RAM_ATTR SetupSwitchChannelData()
{
  uint8_t PacketHeaderAddr;
  Radio.TXdataBuffer[0] = 0b10101001;
  Radio.TXdataBuffer[1] = ((CRSF_to_UINT11(crsf.ChannelDataIn[4]) & 0b1110000000) >> 2) + ((CRSF_to_UINT11(crsf.ChannelDataIn[5]) & 0b1110000000) >> 5) + ((CRSF_to_UINT11(crsf.ChannelDataIn[6]) & 0b1100000000) >> 8);
  Radio.TXdataBuffer[2] = (CRSF_to_UINT11(crsf.ChannelDataIn[6]) & 0b0010000000) + ((CRSF_to_UINT11(crsf.ChannelDataIn[7]) & 0b1110000000) >> 3);
  Radio.TXdataBuffer[3] = Radio.TXdataBuffer[1];
  Radio.TXdataBuffer[4] = Radio.TXdataBuffer[2];
  Radio.TXdataBuffer[5] = Radio.NonceTX; //we use this free byte in switch packet to send the current nonce and sync the RX to the TX
}

void ICACHE_RAM_ATTR SendRCdataToRF()
{
  if (!WaitRXresponse) // only do this logic if we are NOT waiting for the RX to respond
  {

    if ((millis() > (SyncPacketLastSent + SyncPacketInterval)) && isRXconnectionLost)
    {
      SetupSyncPacketData();
      SyncPacketLastSent = millis();
    }
    else
    {
      if ((millis() > (SwitchPacketSendInterval + SwitchPacketLastSent)) || CheckChannels5to8Change())
      {
        SetupSwitchChannelData();
        SwitchPacketLastSent = millis();
      }
      else // else we just have regular channel data which we send as 8 + 2 bits
      {
        Setup4ChannelData();
      }
    }
  }
  //
  uint8_t crc = CalcCRC(Radio.TXdataBuffer, 6);
  Radio.TXdataBuffer[6] = crc;
  //Radio.TXnb(Radio.TXdataBuffer, 7);

  uint8_t modresult = Radio.NonceTX % Radio.ResponseInterval;

  if ((modresult == 0) || (modresult == 1) || (modresult == 2))
  {
    if (modresult == 0)
    {                                     //after the next TX packet we switch to RX mode
      Radio.TXnb(Radio.TXdataBuffer, 7);  //every n packets we tie the txdone callback to the RXnb function to recv a telemetry response
      Radio.TXdoneCallback = &Radio.RXnb; //map the TXdoneCallback to switch to RXnb mode
      //Serial.println("Wait Resp");
      WaitRXresponse = true;
    }
    else if (WaitRXresponse && (modresult == 1)) //need to wait one frame for the response
    {
      WaitRXresponse = false;
      Radio.TXdoneCallback = &Radio.nullCallback;
      Radio.NonceTX++;
      //delayMicroseconds(500);
      //erial.println(".");
      return; // here we are expecting the RX packet so we do nothing and just return, if we got it the callback will fire
    }
    else if (modresult == 2) // the last result, we expect that we either got the response packet or didn't, either way we clear the callback and send a packet normally
    {
      Radio.SetFrequency(FHSSgetNextFreq()); // freq hop before sending the next packet
      Radio.TXnb(Radio.TXdataBuffer, 7);
    }
  }
  else
  {
    Radio.TXnb(Radio.TXdataBuffer, 7); // otherwise the default case is that we just send the TX packet.
  }
}

void setup()
{
  Serial.begin(115200);
  Serial.println("Module Booted...");
  delay(500);

  Radio.RFmodule = RFMOD_SX1278; //define radio module here

  Radio.TXContInterval = 10000; //in microseconds
  Radio.TXbuffLen = 7;
  Radio.RXbuffLen = 7;

  //////Custom Module Pinouts///////
  Radio.SX127x_MISO = 19;
  Radio.SX127x_MOSI = 23;
  Radio.SX127x_SCK = 18;
  Radio.SX127x_dio0 = 26;
  Radio.SX127x_nss = 5;
  Radio.SX127x_RST = 14;
  /////////////////////////

  Radio.SetPreambleLength(2);
  Radio.SetFrequency(433920000); //set frequency first or an error will occur!!!
  Radio.ResponseInterval = 32;
  Radio.RXdoneCallback = &ProcessTLMpacket;

  //Radio.TXdoneCallback = &PrintTest;
  Radio.Begin();

  crsf.RCdataCallback = &SendRCdataToRF;

  crsf.Begin();

  //Radio.StartContTX();
  //Radio.SetOutputPower(0x00);
  Radio.SetOutputPower(0b1111);
  memset((uint16_t *)crsf.ChannelDataIn, 0, 16);
  memset((uint8_t *)Radio.TXdataBuffer, 0, 7);
}

void loop()
{

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
  // Serial.print("PacketCount(HZ): ");
  // Serial.println(Radio.PacketCount);
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

  delay(10);

  if (millis() > (RXconnectionLostTimeout + LastTLMpacketRecvMillis))
  {
    isRXconnectionLost = true;
  }

  if (millis() > (PacketRateLastChecked + PacketRateInterval)) //just some debug data
  {
    PacketRateLastChecked = millis();
    PacketRate = (float)packetCounteRX_TX / (float)(PacketRateInterval);
    linkQuality = (((float)PacketRate / (float)targetFrameRate) * 100000.0);

    if (linkQuality > 99)
    {
      linkQuality = 99;
    }

    packetCounteRX_TX = 0;
  }
}