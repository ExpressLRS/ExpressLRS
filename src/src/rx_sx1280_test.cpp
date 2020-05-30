#include <Arduino.h>
#include "targets.h"
#include "SX1280RadioLib.h"
#include "ESP8266WiFi.h"

SX1280Driver Radio;

uint8_t testdata[8] = {0x80};

void ICACHE_RAM_ATTR TXdoneCallback1()
{
    Serial.println("TXdoneCallback1");
    //delay(1000);
    //Radio.TXnb(testdata, sizeof(testdata));
}

void ICACHE_RAM_ATTR RXdoneCallback1()
{
    Serial.println("RXdoneCallback1");
}

void setup()
{
    Serial.begin(115200);
    Serial.println("Begin SX1280 testing...");

    Radio.Begin();
    Radio.Config(SX1280_LORA_BW_0400, SX1280_LORA_SF10, SX1280_LORA_CR_4_8, 2420000000, SX1280_PREAMBLE_LENGTH_32_BITS);
    Radio.TXdoneCallback1 = &TXdoneCallback1;
    Radio.RXdoneCallback1 = &RXdoneCallback1;
    Radio.TXnb(testdata, sizeof(testdata));
}

void loop()
{

    delay(250);
    Serial.println("about to TX");
    Radio.TXnb(testdata, 8);

    // Serial.println("about to RX");
    // Radio.RXnb();
    // delay(random(50,200));
}


