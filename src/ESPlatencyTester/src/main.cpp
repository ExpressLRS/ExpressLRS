#include <Arduino.h>
#include "CRSF.h"

CRSF crsf(Serial);

#define TRIGGER_WAIT_RAND_MIN_MS 200
#define TRIGGER_WAIR_RAND_MAX_MS 350
uint32_t TriggerBeginTime;

#define GPIO_OUTPUT_PIN D0

uint32_t BeginTriggerMicros;
uint32_t StopTriggerMicros;

uint8_t CurrState;

void ICACHE_RAM_ATTR PrintResults()
{
  Serial.println(StopTriggerMicros - BeginTriggerMicros);
}

void ICACHE_RAM_ATTR RCcallback()
{
  //Serial.println(crsf.ChannelDataIn[2]);
  if (CurrState == 2)
  {
    if (crsf.ChannelDataIn[2] > 1000)
    {
      digitalWrite(D0, LOW);
      StopTriggerMicros = micros();
      PrintResults();
      CurrState = 0;
    }
  }
}

void ICACHE_RAM_ATTR PreTrigger()
{
  TriggerBeginTime = random(TRIGGER_WAIT_RAND_MIN_MS, TRIGGER_WAIR_RAND_MAX_MS) + millis();
  CurrState = 1;
}

void ICACHE_RAM_ATTR DoTrigger()
{
  digitalWrite(D0, HIGH);
  BeginTriggerMicros = micros();
  CurrState = 2;
}

void ICACHE_RAM_ATTR BeginTrigger()
{
}

void setup()
{
  pinMode(D0, OUTPUT);
  pinMode(D1, OUTPUT);
  digitalWrite(D1, LOW);

  CurrState = 0;

  crsf.RCdataCallback1 = &RCcallback;
  crsf.Begin();
  Serial.begin(420000, SERIAL_8N1, SERIAL_FULL, 1, false); // inverted serial
}

void loop()
{
  if (CurrState == 0)
  {
    PreTrigger();
  }
  else if (CurrState == 1)
  {
    if (millis() > TriggerBeginTime)
    {
      DoTrigger();
    }
  }

  crsf.handleUARTin();
}
