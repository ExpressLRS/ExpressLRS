#include <Arduino.h>
#include "targets.h"
#include "SX1280Driver.h"
#include "esp32-hal.h"
#include "common.h"
#include "FHSS.h"

SX1280Driver Radio;

uint8_t testdata[8] = {0x80};

void ICACHE_RAM_ATTR TXdoneCallback()
{
    Serial.println("TXdoneCallback");
    Radio.TXnb(testdata, sizeof(testdata));
}

void ICACHE_RAM_ATTR RXdoneCallback()
{
    Serial.println("RXdoneCallback");
    for (int i = 0; i < 8; i++)
    {
        Serial.print(Radio.RXdataBuffer[i], HEX);
        Serial.print(",");
    }
    Serial.println("");
    //Radio.RXnb();
}

void setup()
{
    Serial.begin(115200);
    Serial.println("Begin SX1280 testing...");

    Radio.Begin();
    //Radio.Config(SX1280_LORA_BW_0800, SX1280_LORA_SF6, SX1280_LORA_CR_4_7, 2420000000, SX1280_PREAMBLE_LENGTH_32_BITS);
    Radio.TXdoneCallback = &TXdoneCallback;
    Radio.RXdoneCallback = &RXdoneCallback;
    Radio.SetFrequencyReg(FHSSfreqs[0]);
    //Radio.RXnb();
    Radio.TXnb(testdata, sizeof(testdata));
}

void loop()
{
    // Serial.println("about to TX");
    
    //delay(1000);

    // Serial.println("about to RX");
    //Radio.RXnb();
    delay(100);
    //delay(random(50,200));
    //delay(100);
    //Radio.GetStatus();

    yield();
}