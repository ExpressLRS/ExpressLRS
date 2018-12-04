#include <Arduino.h>

#include "LoRaRadioLib.h"
#include "PacketLib.h"
#include "CRSF.h"

uint8_t testdata[6] = {1,2,3,4,5,6};
uint32_t TXdoneMicros = 0;

SX127xDriver Radio;

void PrintTest(){
  Serial.println("callbacktest");
  for (int i; i<Radio.RXbuffLen; i++){
    char byteIN = Radio.RXdataBuffer[i];
    Serial.print(byteIN);
  }
}


void setup(){
    Serial.begin(115200);
    Serial.println("Module Booted...");
    delay(1500);

    Radio.RFmodule=RFMOD_SX1278; //define radio module here

    // Radio.SX127x_MISO = 19;
    // Radio.SX127x_MOSI = 27;
    // Radio.SX127x_SCK = 5;
    // Radio.SX127x_dio0 = 26;
    // Radio.SX127x_nss = 18;
    Radio.SX127x_RST = 13;


    Radio.Begin();
    Radio.TXContInterval = 50000;
    Serial.println("Module Booted...");
    Radio.TXbuffLen = 8;
    Radio.SetPreambleLength(2);
    
    Radio.SetFrequency(915000000);

    Radio.RXdoneCallback = &PrintTest;

}

void loop(){
    
  //  for (int i = 0; i<255; i++){
    //    Serial.println(i);
      //  delay(100);
        //Radio.TX(testdata,6);
    //}
    // Serial.print("SPItime: ");
    // Serial.println(Radio.TXspiTime);
    // Serial.print("TotalTime: ");
    // Serial.println(Radio.TimeOnAir);
    // Serial.print("PacketCount(HZ): ");
    // Serial.println(Radio.PacketCount);
    // Radio.PacketCount = 0;
    Radio.StartContTX();
    Serial.println("StartCont");
    delay(2000);
    Radio.StopContTX();
    Serial.println("StopContTX");
    //Radio.StartContRX();
    delay(2000);





}

