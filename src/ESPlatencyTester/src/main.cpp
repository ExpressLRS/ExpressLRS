#include <Arduino.h>

//#define USE_CRSF
//#define USE_SRXL2
#define USE_SBUS

uint16_t ChannelData[16];

#ifdef USE_CRSF
#include "CRSF.h"
CRSF crsf(Serial);
#endif

#ifdef USE_SBUS
#include "SBUS.h"
SBUS sbus(Serial);
bool failSafe;
bool lostFrame;
#endif

#ifdef USE_SRXL2
#include "spm_srxl.h"
#include "spm_srxl_config.h"

void uartInit(uint8_t uartNum, uint32_t baudRate)
{
  Serial.begin(baudRate, SERIAL_8N1, SERIAL_FULL, 1, false);
}

void uartSetBaud(uint8_t uartNum, uint32_t baudRate)
{
  Serial.begin(baudRate, SERIAL_8N1, SERIAL_FULL, 1, false);
}

uint8_t uartReceiveBytes(uint8_t uartNum, uint8_t *pBuffer, uint8_t bufferSize, uint8_t timeout_ms)
{
  Serial.setTimeout(timeout_ms);
  Serial.readBytes(pBuffer, bufferSize);
}

uint8_t uartTransmit(uint8_t uartNum, uint8_t *pBuffer, uint8_t bytesToSend)
{
  Serial.write(pBuffer, bytesToSend);
}

// Forward definitions of app-specific handling of telemetry and channel data -- see examples below
void userProvidedFillSrxlTelemetry(SrxlTelemetryData *pTelemetry)
{
  Serial.println("TLM");
}

void userProvidedReceivedChannelData(SrxlChannelData *pChannelData, bool isFailsafeData)
{
  Serial.println("Data");
}

void userProvidedHandleVtxData(SrxlVtxData *pVtxData)
{
  Serial.println("Data");
}

#endif

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

#ifdef USE_CRSF
void ICACHE_RAM_ATTR CRSFhandle()
{
  ChannelData[2] = crsf.ChannelDataIn[2];
  RCcallback();
}
#endif

void ICACHE_RAM_ATTR RCcallback()
{
  //Serial.println(crsf.ChannelDataIn[2]);
  if (CurrState == 2)
  {
    if (ChannelData[2] > 1000)
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

#ifdef USE_CRSF
  crsf.RCdataCallback1 = &CRSFhandle;
  crsf.Begin();
  Serial.begin(420000, SERIAL_8N1, SERIAL_FULL, 1, false);
#elif defined(USE_SBUS)
  sbus.begin();
#endif
}

#ifdef USE_SBUS
void loop_sbus()
{
  if (sbus.read(&ChannelData[0], &failSafe, &lostFrame))
  {
    RCcallback();
  }
}
#endif

#ifdef USE_SRXL2
void loop_srxl2()
{
  while (Serial.available())
  {
    // Try to receive UART bytes, or timeout after 5 ms
    uint8_t bytesReceived = Serial.read();
    Serial.println(bytesReceived);
    if (bytesReceived)
    {
      rxBufferIndex += bytesReceived;
      if (rxBufferIndex < 5)
        continue;

      if (rxBuffer[0] == SPEKTRUM_SRXL_ID)
      {
        uint8_t packetLength = rxBuffer[2];
        if (rxBufferIndex > packetLength)
        {
          // Try to parse SRXL packet -- this internally calls srxlRun() after packet is parsed and reset timeout
          if (srxlParsePacket(0, rxBuffer, packetLength))
          {
            // Move any remaining bytes to beginning of buffer (usually 0)
            rxBufferIndex -= packetLength;
            memmove(rxBuffer, &rxBuffer[packetLength], rxBufferIndex);
          }
          else
          {
            rxBufferIndex = 0;
          }
        }
      }
    }
    else
    {
      // Tell SRXL state machine that 5 more milliseconds have passed since packet received
      srxlRun(0, 5);
      rxBufferIndex = 0;
    }
    // Check a bind button, and if pressed enter bind mode
    // if (bindButtonPressed)
    // {
    //   srxlEnterBind(DSMX_11MS);
    // }
  }
}
#endif

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

#if defined(USE_CRSF)
  crsf.handleUARTin();
#elif defined(USE_SRXL2)
  loop_srxl2();
#elif defined(USE_SBUS)
  loop_sbus();
#endif
}
