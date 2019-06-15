#include <Arduino.h>
#include "common.h"
#include "LoRaRadioLib.h"
#include "CRSF.h"

uint8_t testdata[5] = {1, 2, 3, 4, 5};
uint32_t TXdoneMicros = 0;

uint8_t SwitchPacketsCounter = 0;        //not used for the moment
uint32_t SwitchPacketSendInterval = 300; //how often to send the switch data packet (ms)
uint32_t SwitchPacketLastSent = 0;       //time in ms when the last switch data packet was sent

uint8_t SwitchData[7] = {0};

SX127xDriver Radio;
CRSF crsf;

void PrintTest()
{
  Serial.println("callbacktest");
  for (int i; i < Radio.RXbuffLen; i++)
  {
    char byteIN = Radio.RXdataBuffer[i];
    Serial.print(byteIN);
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
    if (!(crsf.ChannelDataIn[i] == crsf.ChannelDataIn[i]))
    {
      result = true;
      break;
    }
  }
  return result;
}

void ICACHE_RAM_ATTR SendRCdataToRF()
{
  uint8_t PacketHeaderAddr;
  //const RC_Packet_channels_t *const rcChannels = (RC_Packet_channels_t *)&CRSF::SerialBuffer[SERIAL_PACKET_OFFSET];

  if ((millis() > (SwitchPacketSendInterval + SwitchPacketLastSent)) || CheckChannels5to8Change())
  {
    SwitchPacketLastSent = millis();
    //Serial.println("SwitchData");
    Radio.TXdataBuffer[0] = 0b10101001;
    Radio.TXdataBuffer[1] = ((CRSF_to_UINT11(crsf.ChannelDataIn[4]) & 0b1110000000) >> 2) + ((CRSF_to_UINT11(crsf.ChannelDataIn[5]) & 0b1110000000) >> 5) + ((CRSF_to_UINT11(crsf.ChannelDataIn[6]) & 0b1100000000) >> 8);
    Radio.TXdataBuffer[2] = (CRSF_to_UINT11(crsf.ChannelDataIn[6]) & 0b0010000000) + ((CRSF_to_UINT11(crsf.ChannelDataIn[7]) & 0b1110000000) >> 3);
    Radio.TXdataBuffer[3] = Radio.TXdataBuffer[1];
    Radio.TXdataBuffer[4] = Radio.TXdataBuffer[2];
    Radio.TXdataBuffer[5] = 0;
  }
  else // else we just have regular channel data which we send as 8 + 2 bits
  {
    PacketHeaderAddr = 0b10101000;
    Radio.TXdataBuffer[0] = PacketHeaderAddr;
    Radio.TXdataBuffer[1] = ((CRSF_to_UINT11(crsf.ChannelDataIn[0]) & 0b1111111100) >> 2);
    Radio.TXdataBuffer[2] = ((CRSF_to_UINT11(crsf.ChannelDataIn[1]) & 0b1111111100) >> 2);
    Radio.TXdataBuffer[3] = ((CRSF_to_UINT11(crsf.ChannelDataIn[2]) & 0b1111111100) >> 2);
    Radio.TXdataBuffer[4] = ((CRSF_to_UINT11(crsf.ChannelDataIn[3]) & 0b1111111100) >> 2);
    Radio.TXdataBuffer[5] = ((CRSF_to_UINT11(crsf.ChannelDataIn[0]) & 0b0000000011) << 6) + ((CRSF_to_UINT11(crsf.ChannelDataIn[1]) & 0b0000000011) << 4) +
                            ((CRSF_to_UINT11(crsf.ChannelDataIn[2]) & 0b0000000011) << 2) + ((CRSF_to_UINT11(crsf.ChannelDataIn[3]) & 0b0000000011) << 0);
  }
  //
  uint8_t crc = CalcCRC(Radio.TXdataBuffer, 6);
  Radio.TXdataBuffer[6] = crc;
}

void setup()
{
  Serial.begin(115200);
  Serial.println("Module Booted...");
  delay(500);

  Radio.RFmodule = RFMOD_SX1278; //define radio module here

  Radio.TXContInterval = 5000; //in microseconds
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
  Radio.Begin();

  crsf.RCdataCallback = &SendRCdataToRF;

  crsf.Begin();

  Radio.StartContTX();
  Radio.SetOutputPower(0x00);
}

void loop()
{

  //  for (int i = 0; i<255; i++){
  //    Serial.println(i);
  //delay(100);
  //Serial.println("sending..");
  //Serial.println(Radio.TX(testdata, 5));
  //}
  Serial.print("SPItime: ");
  Serial.println(Radio.TXspiTime);
  Serial.print("TotalTime: ");
  Serial.println(Radio.TimeOnAir);
  Serial.print("HeadRoom: ");
  Serial.println(Radio.HeadRoom);
  Serial.print("PacketCount(HZ): ");
  Serial.println(Radio.PacketCount);
  Radio.PacketCount = 0;
  //PrintRC();
  // Radio.StartContTX();
  // Serial.println("StartCont");
  delay(1000);
  // Radio.StopContTX();
  // Serial.println("StopContTX");
  // Radio.StartContRX();
  // delay(50);
  //PrintRC();
}