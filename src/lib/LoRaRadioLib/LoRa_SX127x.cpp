

#include <Arduino.h>

#include "LoRa_lowlevel.h"
#include "LoRa_SX127x.h"
#include "LoRa_SX1276.h"
#include "LoRa_SX1278.h"
#include "../../src/targets.h"

uint32_t volatile SX127xDriver::PacketCount = 0;

void inline SX127xDriver::nullCallback(void){};

void (*SX127xDriver::RXdoneCallback1)() = &nullCallback;
void (*SX127xDriver::RXdoneCallback2)() = &nullCallback;

void (*SX127xDriver::TXdoneCallback1)() = &nullCallback;
void (*SX127xDriver::TXdoneCallback2)() = &nullCallback;
void (*SX127xDriver::TXdoneCallback3)() = &nullCallback;
void (*SX127xDriver::TXdoneCallback4)() = &nullCallback;

void (*SX127xDriver::TXtimeout)() = &nullCallback;
void (*SX127xDriver::RXtimeout)() = &nullCallback;

void (*SX127xDriver::TimerDoneCallback)() = &nullCallback;

/////setup some default variables//////////////

volatile bool SX127xDriver::headerExplMode = false;

volatile uint32_t SX127xDriver::TimerInterval = 5000;
uint8_t SX127xDriver::TXbuffLen = 8;
uint8_t SX127xDriver::RXbuffLen = 8;

volatile uint8_t SX127xDriver::NonceTX = 0;
volatile uint8_t SX127xDriver::NonceRX = 0;

//////////////////Hardware Pin Variable defaults////////////////
uint8_t SX127xDriver::SX127x_nss = GPIO_PIN_NSS;
uint8_t SX127xDriver::SX127x_dio0 = GPIO_PIN_DIO0;
uint8_t SX127xDriver::SX127x_dio1 = GPIO_PIN_DIO1;

uint8_t SX127xDriver::SX127x_MOSI = GPIO_PIN_MOSI;
uint8_t SX127xDriver::SX127x_MISO = GPIO_PIN_MISO;
uint8_t SX127xDriver::SX127x_SCK = GPIO_PIN_SCK;
uint8_t SX127xDriver::SX127x_RST = GPIO_PIN_RST;

uint8_t SX127xDriver::_RXenablePin = GPIO_PIN_RX_ENABLE;
uint8_t SX127xDriver::_TXenablePin = GPIO_PIN_TX_ENABLE;

#ifdef TARGET_100mW_MODULE
bool SX127xDriver::HighPowerModule = false;
#endif

#ifdef TARGET_1000mW_MODULE
bool SX127xDriver::HighPowerModule = true;
#endif
/////////////////////////////////////////////////////////////////

//// Debug Variables ////
uint32_t SX127xDriver::TimeOnAir = 0;
uint32_t SX127xDriver::LastTXdoneMicros = 0;
uint32_t SX127xDriver::TXdoneMicros = 0;
uint32_t SX127xDriver::TXstartMicros = 0;
int8_t SX127xDriver::LastPacketRSSI = 0;
int8_t SX127xDriver::LastPacketSNR = 0;
uint32_t SX127xDriver::TXspiTime = 0;
uint32_t SX127xDriver::HeadRoom = 0;
/////////////////////////////////////////////////////////////////

Bandwidth SX127xDriver::currBW = BW_500_00_KHZ;
SpreadingFactor SX127xDriver::currSF = SF_6;
CodingRate SX127xDriver::currCR = CR_4_7;
uint8_t SX127xDriver::_syncWord = SX127X_SYNC_WORD;
uint32_t SX127xDriver::currFreq = 0;
uint8_t SX127xDriver::currPWR = 0b0000;
uint8_t SX127xDriver::maxPWR = 0b1111;

uint8_t volatile SX127xDriver::TXdataBuffer[256];
uint8_t volatile SX127xDriver::RXdataBuffer[256];

uint8_t SX127xDriver::currOpmode = 0xFF; // no a valid opmode, ie undefined

ContinousMode SX127xDriver::ContMode = CONT_OFF;
RFmodule_ SX127xDriver::RFmodule = RFMOD_SX1276;

enum InterruptAssignment_
{
  NONE,
  RX_DONE,
  TX_DONE
};

InterruptAssignment_ InterruptAssignment = NONE;
//////////////////////////////////////////////

uint8_t SX127xDriver::Begin()
{
  Serial.println("Driver Begin");
  uint8_t status;

  pinMode(SX127x_RST, OUTPUT);
  digitalWrite(SX127x_RST, 0);
  delay(100);
  digitalWrite(SX127x_RST, 1);
  delay(100);

#if defined(PLATFORM_ESP32)
  if (HighPowerModule)
  {
    pinMode(SX127xDriver::_TXenablePin, OUTPUT);
    pinMode(SX127xDriver::_RXenablePin, OUTPUT);
  }
#endif

  //static uint8_t SX127x_SCK;

  if (RFmodule == RFMOD_SX1278)
  {
    Serial.println("Init SX1278");
    status = SX1278begin(SX127x_nss, SX127x_dio0, SX127x_dio1);
    Serial.println("SX1278 Done");
  }
  else
  {
    Serial.println("Init SX1276");
    status = SX1276begin(SX127x_nss, SX127x_dio0, SX127x_dio1);
    Serial.println("SX1276 Done");
  }

  ConfigLoraDefaults();

  return (status);
}

uint8_t SX127xDriver::SetBandwidth(Bandwidth bw)
{
  uint8_t state = SX127xConfig(bw, currSF, currCR, currFreq, _syncWord);
  if (state == ERR_NONE)
  {
    currBW = bw;
  }
  return (state);
}

uint8_t SX127xDriver::SetSyncWord(uint8_t syncWord)
{

  uint8_t status = setRegValue(SX127X_REG_SYNC_WORD, syncWord);
  if (status != ERR_NONE)
  {
    return (status);
  }
  else
  {
    SX127xDriver::_syncWord = syncWord;
    return (ERR_NONE);
  }
}

uint8_t SX127xDriver::SetOutputPower(uint8_t Power)
{
  //todo make function turn on PA_BOOST ect
  uint8_t status = setRegValue(SX127X_REG_PA_CONFIG, SX127X_PA_SELECT_BOOST | SX127X_MAX_OUTPUT_POWER | Power, 7, 0);

  currPWR = Power;

  if (status != ERR_NONE)
  {
    return (status);
  }
  else
  {
    return (ERR_NONE);
  }
}

uint8_t SX127xDriver::SetPreambleLength(uint8_t PreambleLen)
{
  //status = setRegValue(SX127X_REG_PREAMBLE_MSB, SX127X_PREAMBLE_LENGTH_MSB);
  uint8_t status = setRegValue(SX127X_REG_PREAMBLE_LSB, PreambleLen);
  if (status != ERR_NONE)
  {
    return (status);
  }
  else
  {
    return (ERR_NONE);
  }
}

uint8_t SX127xDriver::SetSpreadingFactor(SpreadingFactor sf)
{
  uint8_t status;
  if (sf == SX127X_SF_6)
  {
    status = setRegValue(SX127X_REG_MODEM_CONFIG_2, SX127X_SF_6 | SX127X_TX_MODE_SINGLE, 7, 3);
    status = setRegValue(SX127X_REG_DETECT_OPTIMIZE, SX127X_DETECT_OPTIMIZE_SF_6, 2, 0);
    status = setRegValue(SX127X_REG_DETECTION_THRESHOLD, SX127X_DETECTION_THRESHOLD_SF_6);
  }
  else
  {
    status = setRegValue(SX127X_REG_MODEM_CONFIG_2, sf | SX127X_TX_MODE_SINGLE, 7, 3);
    status = setRegValue(SX127X_REG_DETECT_OPTIMIZE, SX127X_DETECT_OPTIMIZE_SF_7_12, 2, 0);
    status = setRegValue(SX127X_REG_DETECTION_THRESHOLD, SX127X_DETECTION_THRESHOLD_SF_7_12);
  }
  if (status == ERR_NONE)
  {
    currSF = sf;
  }
  return (status);
}

uint8_t SX127xDriver::SetCodingRate(CodingRate cr)
{
  uint8_t state = SX127xConfig(currBW, currSF, cr, currFreq, _syncWord);
  if (state == ERR_NONE)
  {
    currCR = cr;
  }
  return (state);
}

uint8_t SX127xDriver::SetFrequency(uint32_t freq)
{
  SX127xDriver::currFreq = freq;

  uint8_t status = ERR_NONE;

  status = SetMode(SX127X_STANDBY);

  if (status != ERR_NONE)
  {
    return (status);
  }

#define FREQ_STEP 61.03515625

  int32_t FRQ = ((uint32_t)((double)freq / (double)FREQ_STEP));

  uint8_t FRQ_MSB = (uint8_t)((FRQ >> 16) & 0xFF);
  uint8_t FRQ_MID = (uint8_t)((FRQ >> 8) & 0xFF);
  uint8_t FRQ_LSB = (uint8_t)(FRQ & 0xFF);

  uint8_t outbuff[3] = {FRQ_MSB, FRQ_MID, FRQ_LSB}; //check speedup

  writeRegisterBurst(SX127X_REG_FRF_MSB, outbuff, sizeof(outbuff));

  if (status != ERR_NONE)
  {
    return (status);
  }

  return (status);
}

uint8_t SX127xDriver::SX127xBegin()
{
  uint8_t i = 0;
  bool flagFound = false;
  while ((i < 10) && !flagFound)
  {
    uint8_t version = readRegister(SX127X_REG_VERSION);
    Serial.println(version, HEX);
    if (version == 0x12)
    {
      flagFound = true;
    }
    else
    {
      Serial.print(" not found! (");
      Serial.print(i + 1);
      Serial.print(" of 10 tries) REG_VERSION == ");

      char buffHex[5];
      sprintf(buffHex, "0x%02X", version);
      Serial.print(buffHex);
      Serial.println();
      delay(200);
      i++;
    }
  }

  if (!flagFound)
  {
    Serial.println(" not found!");
    return (ERR_CHIP_NOT_FOUND);
  }
  else
  {
    Serial.println(" found! (match by REG_VERSION == 0x12)");
  }
  return (ERR_NONE);
}

///////////////////////////////////Functions for Hardware Timer/////////////////////////////////////////
#ifdef PLATFORM_ESP32

//on the ESP32 we can use a hardware timer to do fancy things like schedule regular TX transmissions

hw_timer_t *timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;
TaskHandle_t TimerTask_handle = NULL;

void ICACHE_RAM_ATTR SX127xDriver::TimerTask_ISRhandler()
{
  portMUX_TYPE myMutex = portMUX_INITIALIZER_UNLOCKED;
  portENTER_CRITICAL(&myMutex);
  TimerDoneCallback();
  portEXIT_CRITICAL(&myMutex);
}

void ICACHE_RAM_ATTR SX127xDriver::StopTimerTask()
{
  //vTaskSuspend(TimerTask_handle);
  detachInterrupt(SX127xDriver::SX127x_dio0);
  //SX127xDriver::SetMode(SX127X_SLEEP);
  ClearIRQFlags();

  if (timer)
  {
    timerEnd(timer);
    timer = NULL;
  }

  //timerEnd(timer);
}

void ICACHE_RAM_ATTR SX127xDriver::UpdateTimerInterval()
{
  if (timer)
  {
    timerAlarmWrite(timer, TimerInterval, true);
  }
}

void ICACHE_RAM_ATTR SX127xDriver::StartTimerTask()
{
  if (!timer)
  {
    timer = timerBegin(0, 80, true);
    timerAttachInterrupt(timer, &TimerTask_ISRhandler, true);
    timerAlarmWrite(timer, TimerInterval, true);
    timerAlarmEnable(timer);
  }
}
#endif
////////////////////////////////////////////////////////////////////////////////////////

void SX127xDriver::ConfigLoraDefaults()
{
  Serial.print("Setting ExpressLRS LoRa reg defaults");
  writeRegister(SX127X_REG_PAYLOAD_LENGTH, TXbuffLen);
  writeRegister(SX127X_REG_FIFO_TX_BASE_ADDR, SX127X_FIFO_TX_BASE_ADDR_MAX);
  writeRegister(SX127X_REG_FIFO_RX_BASE_ADDR, SX127X_FIFO_RX_BASE_ADDR_MAX);
  setRegValue(SX127X_REG_DIO_MAPPING_1, 0b11000000, 7, 6); //undocumented "hack", looking at Table 18 from datasheet SX127X_REG_DIO_MAPPING_1 = 11 appears to be unspported by infact it generates an intterupt on both RXdone and TXdone, this saves switching modes.
}

/////////////////////////////////////TX functions/////////////////////////////////////////

void ICACHE_RAM_ATTR SX127xDriver::TXnbISR()
{

#if defined(PLATFORM_ESP32)
  digitalWrite(_TXenablePin, LOW); //the larger TX/RX modules require that the TX/RX enable pins are toggled
#endif
  ClearIRQFlags();

  NonceTX++;
  TXdoneCallback1();
  TXdoneCallback2();
  TXdoneCallback3();
  TXdoneCallback4();
  TXdoneMicros = micros();
}

uint8_t ICACHE_RAM_ATTR SX127xDriver::TXnb(const volatile uint8_t *data, uint8_t length)
{

  SX127xDriver::TXstartMicros = micros();
  SX127xDriver::HeadRoom = TXstartMicros - TXdoneMicros;
  //ClearIRQFlags();
  SetMode(SX127X_STANDBY);

  if (InterruptAssignment != TX_DONE)
  {
    attachInterrupt(digitalPinToInterrupt(SX127x_dio0), TXnbISR, RISING);
    InterruptAssignment = TX_DONE;
  }

  setRegValue(SX127X_REG_FIFO_ADDR_PTR, SX127X_FIFO_TX_BASE_ADDR_MAX);
  writeRegisterBurst((uint8_t)SX127X_REG_FIFO, (uint8_t *)data, length);

#if defined(PLATFORM_ESP32)

  digitalWrite(_RXenablePin, LOW);
  digitalWrite(_TXenablePin, HIGH); //the larger TX/RX modules require that the TX/RX enable pins are toggled

#endif

  SetMode(SX127X_TX);
  PacketCount = PacketCount + 1;

  return (ERR_NONE);
}

///////////////////////////////////RX Functions Non-Blocking///////////////////////////////////////////

void ICACHE_RAM_ATTR SX127xDriver::RXnbISR()
{
  ClearIRQFlags();
  readRegisterBurst((uint8_t)SX127X_REG_FIFO, (uint8_t)RXbuffLen, (uint8_t *)RXdataBuffer);
  SX127xDriver::LastPacketRSSI = SX127xDriver::GetLastPacketRSSI();
  SX127xDriver::LastPacketSNR = SX127xDriver::GetLastPacketSNR();
  NonceRX++;
  RXdoneCallback1();
  RXdoneCallback2();
}

void ICACHE_RAM_ATTR SX127xDriver::StopContRX()
{
  detachInterrupt(SX127xDriver::SX127x_dio0);
  SX127xDriver::SetMode(SX127X_SLEEP);
  ClearIRQFlags();
  InterruptAssignment = NONE;
}

void ICACHE_RAM_ATTR SX127xDriver::RXnb()
{
  //RX continuous mode

#if defined(PLATFORM_ESP32)
  digitalWrite(_TXenablePin, LOW); //the larger TX/RX modules require that the TX/RX enable pins are toggled
  digitalWrite(_RXenablePin, HIGH);
#endif

  SetMode(SX127X_STANDBY);

  if (InterruptAssignment != RX_DONE) //attach interrupt to DIO0,
  {
    attachInterrupt(digitalPinToInterrupt(SX127x_dio0), RXnbISR, RISING);
    InterruptAssignment = RX_DONE;
  }

  ClearIRQFlags();

  if (headerExplMode == false)
  {
    setRegValue(SX127X_REG_PAYLOAD_LENGTH, RXbuffLen);
  }

  setRegValue(SX127X_REG_FIFO_ADDR_PTR, SX127X_FIFO_RX_BASE_ADDR_MAX);
  SetMode(SX127X_RXCONTINUOUS);
}

uint8_t SX127xDriver::RunCAD()
{
  SetMode(SX127X_STANDBY);

  setRegValue(SX127X_REG_DIO_MAPPING_1, SX127X_DIO0_CAD_DONE | SX127X_DIO1_CAD_DETECTED, 7, 4);

  SetMode(SX127X_CAD);
  ClearIRQFlags();

  uint32_t startTime = millis();

  while (!digitalRead(SX127x_dio0))
  {
    if (millis() > (startTime + 500))
    {
      return (CHANNEL_FREE);
    }
    else
    {
      //yield();
      if (digitalRead(SX127x_dio1))
      {
        ClearIRQFlags();
        return (PREAMBLE_DETECTED);
      }
    }
  }

  ClearIRQFlags();
  return (CHANNEL_FREE);
}

uint8_t ICACHE_RAM_ATTR SX127xDriver::SetMode(uint8_t mode)
{ //if radio is not already in the required mode set it to the requested mod
  if (currOpmode != mode)
  {
    setRegValue(SX127X_REG_OP_MODE, mode, 2, 0);
    currOpmode = mode;
  }
  return (ERR_NONE);
}

uint8_t SX127xDriver::Config(Bandwidth bw, SpreadingFactor sf, CodingRate cr, uint32_t freq, uint8_t syncWord)
{
  if (RFmodule == RFMOD_SX1276)
  {
    SX1276config(bw, sf, cr, freq, syncWord);
  }

  if (RFmodule == RFMOD_SX1278)
  {
    SX1278config(bw, sf, cr, freq, syncWord);
  }
  return 0;
}

uint8_t SX127xDriver::SX127xConfig(uint8_t bw, uint8_t sf, uint8_t cr, uint32_t freq, uint8_t syncWord)
{

  uint8_t status = ERR_NONE;

  // set mode to SLEEP
  status = SetMode(SX127X_SLEEP);
  if (status != ERR_NONE)
  {
    return (status);
  }

  // set LoRa mode
  status = setRegValue(SX127X_REG_OP_MODE, SX127X_LORA, 7, 7);
  if (status != ERR_NONE)
  {
    return (status);
  }

  SetFrequency(freq);

  // output power configuration

  status = setRegValue(SX127X_REG_PA_CONFIG, SX127X_PA_SELECT_BOOST | SX127X_MAX_OUTPUT_POWER | currPWR);
  //status = setRegValue(SX127X_REG_PA_CONFIG, SX127X_PA_SELECT_BOOST | SX127X_OUTPUT_POWER);
  status = setRegValue(SX127X_REG_OCP, SX127X_OCP_ON | 23, 5, 0); //200ma
  //status = setRegValue(SX127X_REG_LNA, SX127X_LNA_GAIN_1 | SX127X_LNA_BOOST_ON);

  if (status != ERR_NONE)
  {
    return (status);
  }

  // turn off frequency hopping
  status = setRegValue(SX127X_REG_HOP_PERIOD, SX127X_HOP_PERIOD_OFF);
  if (status != ERR_NONE)
  {
    return (status);
  }

  // basic setting (bw, cr, sf, header mode and CRC)
  if (sf == SX127X_SF_6)
  {
    status = setRegValue(SX127X_REG_MODEM_CONFIG_2, SX127X_SF_6 | SX127X_TX_MODE_SINGLE, 7, 3);
    //status = setRegValue(SX127X_REG_MODEM_CONFIG_2, SX127X_SF_6 | SX127X_TX_MODE_CONT, 7, 3);
    status = setRegValue(SX127X_REG_DETECT_OPTIMIZE, SX127X_DETECT_OPTIMIZE_SF_6, 2, 0);
    status = setRegValue(SX127X_REG_DETECTION_THRESHOLD, SX127X_DETECTION_THRESHOLD_SF_6);
  }
  else
  {
    status = setRegValue(SX127X_REG_MODEM_CONFIG_2, sf | SX127X_TX_MODE_SINGLE, 7, 3);
    //status = setRegValue(SX127X_REG_MODEM_CONFIG_2, sf | SX127X_TX_MODE_CONT, 7, 3);
    status = setRegValue(SX127X_REG_DETECT_OPTIMIZE, SX127X_DETECT_OPTIMIZE_SF_7_12, 2, 0);
    status = setRegValue(SX127X_REG_DETECTION_THRESHOLD, SX127X_DETECTION_THRESHOLD_SF_7_12);
  }

  if (status != ERR_NONE)
  {
    return (status);
  }

  // set the sync word
  status = setRegValue(SX127X_REG_SYNC_WORD, syncWord);
  if (status != ERR_NONE)
  {
    return (status);
  }

  // set default preamble length
  //status = setRegValue(SX127X_REG_PREAMBLE_MSB, SX127X_PREAMBLE_LENGTH_MSB);
  //status = setRegValue(SX127X_REG_PREAMBLE_LSB, SX127X_PREAMBLE_LENGTH_LSB);

  //status = setRegValue(SX127X_REG_PREAMBLE_MSB, 0);
  //status = setRegValue(SX127X_REG_PREAMBLE_LSB, 6);

  if (status != ERR_NONE)
  {
    return (status);
  }

  // set mode to STANDBY
  status = SetMode(SX127X_STANDBY);
  return (status);
}

uint32_t ICACHE_RAM_ATTR SX127xDriver::getCurrBandwidth()
{

  switch (SX127xDriver::currBW)
  {
  case 0:
    return 7.8E3;
  case 1:
    return 10.4E3;
  case 2:
    return 15.6E3;
  case 3:
    return 20.8E3;
  case 4:
    return 31.25E3;
  case 5:
    return 41.7E3;
  case 6:
    return 62.5E3;
  case 7:
    return 125E3;
  case 8:
    return 250E3;
  case 9:
    return 500E3;
  }

  return -1;
}

uint32_t ICACHE_RAM_ATTR SX127xDriver::getCurrBandwidthNormalisedShifted() // this is basically just used for speedier calc of the freq offset, pre compiled for 32mhz xtal
{

  switch (SX127xDriver::currBW)
  {
  case 0:
    return 1026;
  case 1:
    return 769;
  case 2:
    return 513;
  case 3:
    return 385;
  case 4:
    return 256;
  case 5:
    return 192;
  case 6:
    return 128;
  case 7:
    return 64;
  case 8:
    return 32;
  case 9:
    return 16;
  }

  return -1;
}

void ICACHE_RAM_ATTR SX127xDriver::setPPMoffsetReg(int32_t offset)
{
  int32_t offsetValue = ((int32_t)243) * (offset << 8) / ((((int32_t)SX127xDriver::currFreq / 1000000)) << 8);
  offsetValue >>= 8;

  uint8_t regValue = offsetValue & 0b01111111;

  if (offsetValue < 0)
  {
    regValue = regValue | 0b10000000; //set neg bit for 2s complement
  }

  writeRegister(SX127x_PPMOFFSET, regValue);
}

bool SX127xDriver::GetFrequencyErrorbool()
{
  return (readRegister(SX127X_REG_FEI_MSB) & 0b1000) >> 3; // returns true if pos freq error, neg if false
}

int32_t SX127xDriver::GetFrequencyError()
{

  uint8_t reg[3] = {0x0, 0x0, 0x0};
  readRegisterBurst(SX127X_REG_FEI_MSB, sizeof(reg), reg);

  uint32_t RegFei = ((reg[0] & 0b0111) << 16) + (reg[1] << 8) + reg[2];

  int32_t intFreqError = RegFei;

  if ((reg[0] & 0b1000) >> 3)
  {
    intFreqError -= 524288; // Sign bit is on
  }

  int32_t fErrorHZ = (intFreqError >> 3) * (SX127xDriver::getCurrBandwidthNormalisedShifted()); // bit shift hackery so we don't have to use floaty bois; the >> 3 is intentional and is a simplification of the formula on page 114 of sx1276 datasheet
  fErrorHZ >>= 4;

  return fErrorHZ;
}

uint8_t ICACHE_RAM_ATTR SX127xDriver::UnsignedGetLastPacketRSSI()
{
  return (getRegValue(SX127X_REG_PKT_RSSI_VALUE));
}

int8_t ICACHE_RAM_ATTR SX127xDriver::GetLastPacketRSSI()
{
  return (-157 + getRegValue(SX127X_REG_PKT_RSSI_VALUE));
}

int8_t ICACHE_RAM_ATTR SX127xDriver::GetCurrRSSI()
{
  return (-157 + getRegValue(SX127X_REG_RSSI_VALUE));
}

int8_t ICACHE_RAM_ATTR SX127xDriver::GetLastPacketSNR()
{
  int8_t rawSNR = (int8_t)getRegValue(SX127X_REG_PKT_SNR_VALUE);
  return (rawSNR / 4.0);
}

void ICACHE_RAM_ATTR SX127xDriver::ClearIRQFlags()
{
  writeRegister(SX127X_REG_IRQ_FLAGS, 0b11111111);
}
