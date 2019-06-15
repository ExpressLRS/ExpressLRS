#include <Arduino.h>
#include "common.h"
#include "LoRaRadioLib.h"
#include "CRSF.h"

#include "ESP8266_WebUpdate.h"

//#define PLATFORM_ESP8266
//#undef PLATFORM_ESP32

uint8_t testdata[6] = {1, 2, 3, 4, 5, 6};
// uint32_t TXdoneMicros = 0;

extern uint16_t UStoCRSF(uint16_t DataIn);

bool LED = false;

SX127xDriver Radio;
CRSF crsf(Serial);

int packetCounter = 0;
uint32_t PacketRateLastChecked = 0;
uint32_t PacketRateInterval = 250;
float PacketRate = 0.0;
float linkQuality = 0.0;
uint16_t targetFrameRate = 200;

void PrintTest()
{
    Serial.println("callbacktest");
    for (int i; i < Radio.RXbuffLen; i++)
    {
        char byteIN = Radio.RXdataBuffer[i];
        Serial.print(byteIN);
    }
}

void getRFlinkInfo()
{
    crsf.LinkStatistics.uplink_RSSI_1 = Radio.GetLastPacketRSSI();
    crsf.LinkStatistics.uplink_RSSI_2 = Radio.GetLastPacketRSSI();
    crsf.LinkStatistics.uplink_SNR = Radio.GetLastPacketSNR();
    crsf.LinkStatistics.uplink_Link_quality = linkQuality;

    crsf.sendLinkStatisticsToFC();

    digitalWrite(16, LED);
    LED = !LED;
}

void SendRCframe()
{

    uint8_t calculatedCRC = CalcCRC(Radio.RXdataBuffer, 6);
    uint8_t inCRC = Radio.RXdataBuffer[6];
    uint8_t header = Radio.RXdataBuffer[0];
    if (header == 0b10101001 || header == 0b10101000)
    {
        if ((inCRC == calculatedCRC))
        {
            if (header == 0b10101000)
            {
                crsf.PackedRCdataOut.ch0 = UINT11_to_CRSF((Radio.RXdataBuffer[1] << 2) + (Radio.RXdataBuffer[5] & 0b11000000 >> 6));
                crsf.PackedRCdataOut.ch1 = UINT11_to_CRSF((Radio.RXdataBuffer[2] << 2) + (Radio.RXdataBuffer[5] & 0b00110000 >> 4));
                crsf.PackedRCdataOut.ch2 = UINT11_to_CRSF((Radio.RXdataBuffer[3] << 2) + (Radio.RXdataBuffer[5] & 0b00001100 >> 2));
                crsf.PackedRCdataOut.ch3 = UINT11_to_CRSF((Radio.RXdataBuffer[4] << 2) + (Radio.RXdataBuffer[5] & 0b00000011 >> 0));
            }

            if (header == 0b10101001)
            {
                digitalWrite(16, LED);
                LED = !LED;
                //return;
                if ((Radio.RXdataBuffer[3] == Radio.RXdataBuffer[1]) && (Radio.RXdataBuffer[4] == Radio.RXdataBuffer[2])) // extra layer of protection incase the crc and addr headers fail us.
                {

                    // crsf.PackedRCdataOut.ch4 = UINT11_to_CRSF((uint16_t)(Radio.RXdataBuffer[1] & 0b11100000) << 2); //unpack the byte structure, each switch is stored as a possible 8 states (3 bits). we shift by 2 to translate it into the 0....1024 range like the other channel data.
                    // crsf.PackedRCdataOut.ch5 = UINT11_to_CRSF((uint16_t)(Radio.RXdataBuffer[1] & 0b00011100) << (3 + 2));
                    // crsf.PackedRCdataOut.ch6 = UINT11_to_CRSF((uint16_t)((Radio.RXdataBuffer[1] & 0b00000011) << (6 + 2)) + (Radio.RXdataBuffer[2] & 0b10000000));
                    // crsf.PackedRCdataOut.ch7 = UINT11_to_CRSF((uint16_t)((Radio.RXdataBuffer[2] & 0b01110000) << 3));

                    crsf.PackedRCdataOut.ch4 = SWITCH3b_to_CRSF((uint16_t)(Radio.RXdataBuffer[1] & 0b11100000) >> 5); //unpack the byte structure, each switch is stored as a possible 8 states (3 bits). we shift by 2 to translate it into the 0....1024 range like the other channel data.
                    crsf.PackedRCdataOut.ch5 = SWITCH3b_to_CRSF((uint16_t)(Radio.RXdataBuffer[1] & 0b00011100) >> 2);
                    crsf.PackedRCdataOut.ch6 = SWITCH3b_to_CRSF((uint16_t)((Radio.RXdataBuffer[1] & 0b00000011) << 1) + ((Radio.RXdataBuffer[2] & 0b10000000) >> 7));
                    crsf.PackedRCdataOut.ch7 = SWITCH3b_to_CRSF((uint16_t)((Radio.RXdataBuffer[2] & 0b01110000) >> 4));
                }
            }
            crsf.sendRCFrameToFC();
            packetCounter++;
        }
        else
        {
        }
    }
}

void setup()
{
    delay(500);
    pinMode(16, OUTPUT);
    BeginWebUpdate();

    delay(200);
    digitalWrite(16, HIGH);
    delay(200);
    digitalWrite(16, LOW);
    delay(200);
    digitalWrite(16, HIGH);
    delay(200);
    digitalWrite(16, LOW);

    Radio.RFmodule = RFMOD_SX1278; //define radio module here
    Radio.TXContInterval = 10000;
    Radio.TXbuffLen = 7;
    Radio.RXbuffLen = 7;

    //////Custom Module/////// ExpressLRS 20x20mm V2
    Radio.SX127x_MISO = 12;
    Radio.SX127x_MOSI = 13;
    Radio.SX127x_SCK = 14;
    Radio.SX127x_dio0 = 4;
    Radio.SX127x_nss = 15;
    Radio.SX127x_RST = 2;
    /////////////////////////

    Serial.begin(420000);
    Serial.println("Module Booted...");
    crsf.InitSerial();

    Radio.SetPreambleLength(2);
    Radio.SetFrequency(433920000);
    Radio.Begin();

    Radio.RXdoneCallback = &SendRCframe;
    Radio.StartContRX();
    crsf.Begin();

    //Radio.StartContTX();
}

void loop()
{

    //  for (int i = 0; i<255; i++){
    //    Serial.println(i);
    //delay(100);
    //Serial.println("sending..");
    //Serial.println(Radio.TX(testdata, 6));
    //}
    //Serial.print("SPItime: ");
    //Serial.println(Radio.TXspiTime);
    // Serial.print("TotalTime: ");
    // Serial.println(Radio.TimeOnAir);
    // Serial.print("PacketCount(HZ): ");
    // Serial.println(Radio.PacketCount);
    // Radio.PacketCount = 0;
    // Radio.StartContTX();
    // Serial.println("StartCont");
    //uint8_t inData[6] = {0};

    //Radio.RXsingle(inData, 6);

    //delay(500);
    //Serial.println(packetCounter*2);
    //packetCounter = 0;

    //digitalWrite(16, LED);
    //LED = !LED;

    HandleWebUpdate();
    // Radio.StopContTX();
    // Serial.println("StopContTX");
    // Radio.StartContRX();
    // delay(50);
    //PrintRC();
    if (millis() > (PacketRateLastChecked + PacketRateInterval)) //just some debug data
    {
        PacketRateLastChecked = millis();
        PacketRate = (float)packetCounter / (float)(PacketRateInterval);
        linkQuality = (((float)PacketRate / (float)targetFrameRate) * 100000.0);
        packetCounter = 0;
        //getRFlinkInfo();

        crsf.PackedRCdataOut.ch8 = UINT11_to_CRSF(map(Radio.GetLastPacketRSSI(), -100, -50, 0, 1023));
        crsf.PackedRCdataOut.ch9 = UINT11_to_CRSF(fmap(linkQuality, 0, targetFrameRate, 0, 1023));
        crsf.LinkStatistics.uplink_RSSI_1 = map(Radio.GetLastPacketRSSI(), -100, 0, 0, 255);
        //crsf.sendRCFrameToFC();

        //crsf.sendRCFrameToFC();
        //crsf.sendLinkStatisticsToFC();
    }
}