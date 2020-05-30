#include <Arduino.h>
#include "targets.h"
#include "SX1280RadioLib.h"
#include "esp32-hal.h"
#include "common.h"

SX1280Driver Radio;

uint8_t testdata[8] = {0x80};

void ICACHE_RAM_ATTR TXdoneCallback1()
{
    Serial.println("TXdoneCallback1");
}

void ICACHE_RAM_ATTR RXdoneCallback1()
{
    Serial.println("RXdoneCallback1");
    //Radio.RXnb();
}

void setup()
{
    Serial.begin(115200);
    Serial.println("Begin SX1280 testing...");

    Radio.Begin();
    Radio.TXdoneCallback1 = &TXdoneCallback1;
    Radio.RXdoneCallback1 = &RXdoneCallback1;
    Radio.RXnb();
}

void loop()
{
    // Serial.println("about to TX");
    //Radio.TXnb(testdata, sizeof(testdata));
    //delay(1000);

    // Serial.println("about to RX");
    //Radio.RXnb();
    //delay(1000);
    //delay(random(50,200));
    //delay(100);
    //Radio.GetStatus();

    yield();
}