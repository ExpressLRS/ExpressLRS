#include <Arduino.h>
#include "targets.h"
#include "SX1280Driver.h"
#include "ESP8266WiFi.h"
#include "FHSS.h"

SX1280Driver Radio;

uint8_t testdata[8] = {0x80};

void ICACHE_RAM_ATTR TXdoneCallback()
{
    Serial.println("TXdoneCallback1");
    //delay(1000);
    //Radio.TXnb(testdata, sizeof(testdata));
}

void ICACHE_RAM_ATTR RXdoneCallback(SX12xxDriverCommon::rx_status const /*status*/)
{
    Serial.println("RXdoneCallback");
}

void setup()
{
    Serial.begin(115200);
    Serial.println("Begin SX1280 testing...");

    Radio.Begin();
    Radio.Config(SX1280_LORA_BW_0800, SX1280_LORA_SF6, SX1280_LORA_CR_4_7, 2420000000, 12, 8);
    Radio.TXdoneCallback = &TXdoneCallback;
    //Radio.TXnb(testdata, sizeof(testdata));
    Radio.SetFrequencyReg(FHSSfreqs[0]);
    Radio.RXnb();
}

void loop()
{

    //delay(250);
    //Serial.println("about to TX");
    //Radio.TXnb(testdata, 8);

    // Serial.println("about to RX");
    // Radio.RXnb();
    // delay(random(50,200));
}


