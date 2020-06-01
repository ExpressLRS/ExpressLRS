#include "LoRa_SX127x.h"

//SX127xHal hal(GPIO_PIN_MISO, GPIO_PIN_MOSI, GPIO_PIN_SCK, GPIO_PIN_NSS, GPIO_PIN_RST, GPIO_PIN_DIO0, GPIO_PIN_DIO1, GPIO_PIN_RX_ENABLE, GPIO_PIN_TX_ENABLE);
SX127xHal hal;

void inline SX127xDriver::nullCallback(void){};
SX127xDriver *SX127xDriver::instance = NULL;

void (*SX127xDriver::RXdoneCallback1)() = &nullCallback;
void (*SX127xDriver::RXdoneCallback2)() = &nullCallback;

void (*SX127xDriver::TXdoneCallback1)() = &nullCallback;
void (*SX127xDriver::TXdoneCallback2)() = &nullCallback;
void (*SX127xDriver::TXdoneCallback3)() = &nullCallback;
void (*SX127xDriver::TXdoneCallback4)() = &nullCallback;

void (*SX127xDriver::TXtimeout)() = &nullCallback;
void (*SX127xDriver::RXtimeout)() = &nullCallback;

volatile WORD_ALIGNED_ATTR uint8_t SX127xDriver::TXdataBuffer[TXRXBuffSize] = {0};
volatile WORD_ALIGNED_ATTR uint8_t SX127xDriver::RXdataBuffer[TXRXBuffSize] = {0};
//////////////////////////////////////////////

SX127xDriver::SX127xDriver()
{
  SetMode(SX127x_OPMODE_SLEEP);
  instance = this;
}

void SX127xDriver::Begin()
{
  Serial.println("SX127x Driver Begin");
  hal.TXdoneCallback = &TXnbISR;
  hal.RXdoneCallback = &RXnbISR;
  hal.init();
  DetectChip();
  ConfigLoraDefaults();
}

void SX127xDriver::End()
{
  hal.end();
  instance->TXdoneCallback1 = &nullCallback;
  instance->TXdoneCallback2 = &nullCallback;
  instance->TXdoneCallback3 = &nullCallback;
  instance->TXdoneCallback4 = &nullCallback;

  instance->RXdoneCallback1 = &nullCallback;
  instance->RXdoneCallback2 = &nullCallback;
}

void SX127xDriver::ConfigLoraDefaults()
{
  Serial.println("Setting ExpressLRS LoRa reg defaults");

  SetMode(SX127x_OPMODE_SLEEP);
  hal.writeRegister(SX127X_REG_OP_MODE, ModFSKorLoRa); //must be written in sleep mode
  SetMode(SX127x_OPMODE_STANDBY);

  hal.writeRegister(SX127X_REG_PAYLOAD_LENGTH, TXbuffLen);
  SetSyncWord(currSyncWord);
  hal.writeRegister(SX127X_REG_FIFO_TX_BASE_ADDR, SX127X_FIFO_TX_BASE_ADDR_MAX);
  hal.writeRegister(SX127X_REG_FIFO_RX_BASE_ADDR, SX127X_FIFO_RX_BASE_ADDR_MAX);
  hal.setRegValue(SX127X_REG_DIO_MAPPING_1, 0b11000000, 7, 6); //undocumented "hack", looking at Table 18 from datasheet SX127X_REG_DIO_MAPPING_1 = 11 appears to be unspported by infact it generates an intterupt on both RXdone and TXdone, this saves switching modes.
  hal.writeRegister(SX127X_REG_LNA, SX127X_LNA_BOOST_ON);
  hal.writeRegister(SX1278_REG_MODEM_CONFIG_3, SX1278_AGC_AUTO_ON | SX1278_LOW_DATA_RATE_OPT_OFF);
  hal.setRegValue(SX127X_REG_OCP, SX127X_OCP_ON | SX127X_OCP_150MA, 5, 0); //150ma max current
  SetPreambleLength(SX127X_PREAMBLE_LENGTH_LSB);
}

void SX127xDriver::SetBandwidthCodingRate(SX127x_Bandwidth bw, SX127x_CodingRate cr)
{
  if ((currBW != bw) || (currCR != cr))
  {
    if (currSF == SX127x_SF_6) // set SF6 optimizations
    {
      hal.writeRegister(SX127X_REG_MODEM_CONFIG_1, bw | cr | SX1278_HEADER_IMPL_MODE);
      hal.setRegValue(SX127X_REG_MODEM_CONFIG_2, SX1278_RX_CRC_MODE_OFF, 2, 2);
    }
    else
    {
      if (headerExplMode)
      {
        hal.writeRegister(SX127X_REG_MODEM_CONFIG_1, bw | cr | SX1278_HEADER_EXPL_MODE);
      }
      else
      {
        hal.writeRegister(SX127X_REG_MODEM_CONFIG_1, bw | cr | SX1278_HEADER_IMPL_MODE);
      }

      if (crcEnabled)
      {
        hal.setRegValue(SX127X_REG_MODEM_CONFIG_2, SX1278_RX_CRC_MODE_ON, 2, 2);
      }
      else
      {
        hal.setRegValue(SX127X_REG_MODEM_CONFIG_2, SX1278_RX_CRC_MODE_OFF, 2, 2);
      }
    }

    if (bw == SX127x_BW_500_00_KHZ)
    {
      //datasheet errata reconmendation http://caxapa.ru/thumbs/972894/SX1276_77_8_ErrataNote_1.1_STD.pdf
      hal.writeRegister(0x36, 0x02);
      hal.writeRegister(0x3a, 0x64);
    }
    else
    {
      hal.writeRegister(0x36, 0x03);
    }
    currCR = cr;
    currBW = bw;
  }
}

void SX127xDriver::SetSyncWord(uint8_t syncWord)
{
  // if (currSyncWord != syncWord)
  //{
  hal.writeRegister(SX127X_REG_SYNC_WORD, syncWord);
  currSyncWord = syncWord;
  //}
}

void SX127xDriver::SetOutputPower(uint8_t Power)
{
  //todo make function turn on PA_BOOST ect
  //if (currPWR != Power)
  //{
  hal.writeRegister(SX127X_REG_PA_CONFIG, SX127X_PA_SELECT_BOOST | SX127X_MAX_OUTPUT_POWER | Power);
  currPWR = Power;
  //}
}

void SX127xDriver::SetPreambleLength(uint8_t PreambleLen)
{
  if (currPreambleLen != PreambleLen)
  {
    hal.writeRegister(SX127X_REG_PREAMBLE_LSB, PreambleLen);
    currPreambleLen = PreambleLen;
  }
}

void SX127xDriver::SetSpreadingFactor(SX127x_SpreadingFactor sf)
{
  if (currSF != sf)
  {
    hal.setRegValue(SX127X_REG_MODEM_CONFIG_2, sf | SX127X_TX_MODE_SINGLE, 7, 3);
    if (sf == SX127x_SF_6)
    {
      hal.setRegValue(SX127X_REG_DETECT_OPTIMIZE, SX127X_DETECT_OPTIMIZE_SF_6, 2, 0);
      hal.writeRegister(SX127X_REG_DETECTION_THRESHOLD, SX127X_DETECTION_THRESHOLD_SF_6);
    }
    else
    {
      hal.setRegValue(SX127X_REG_DETECT_OPTIMIZE, SX127X_DETECT_OPTIMIZE_SF_7_12, 2, 0);
      hal.writeRegister(SX127X_REG_DETECTION_THRESHOLD, SX127X_DETECTION_THRESHOLD_SF_7_12);
    }
    currSF = sf;
  }
}

void SX127xDriver::SetFrequency(uint32_t freq)
{
  currFreq = freq;
  SetMode(SX127x_OPMODE_STANDBY);

#define FREQ_STEP 61.03515625

  int32_t FRQ = ((uint32_t)((double)freq / (double)FREQ_STEP));

  uint8_t FRQ_MSB = (uint8_t)((FRQ >> 16) & 0xFF);
  uint8_t FRQ_MID = (uint8_t)((FRQ >> 8) & 0xFF);
  uint8_t FRQ_LSB = (uint8_t)(FRQ & 0xFF);

  WORD_ALIGNED_ATTR uint8_t outbuff[3] = {FRQ_MSB, FRQ_MID, FRQ_LSB}; //check speedup

  hal.writeRegisterBurst(SX127X_REG_FRF_MSB, outbuff, sizeof(outbuff));
}

void SX127xDriver::DetectChip()
{
  uint8_t i = 0;
  bool flagFound = false;
  while ((i < 10) && !flagFound)
  {
    uint8_t version = hal.readRegister(SX127X_REG_VERSION);
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
  }
  else
  {
    Serial.println(" found! (match by REG_VERSION == 0x12)");
  }
  hal.setRegValue(SX127X_REG_OP_MODE, SX127x_OPMODE_SLEEP, 2, 0);
}

/////////////////////////////////////TX functions/////////////////////////////////////////

void ICACHE_RAM_ATTR SX127xDriver::TXnbISR()
{
  hal.TXRXdisable();
  instance->IRQneedsClear = true;
  instance->currOpmode = SX127x_OPMODE_STANDBY; //goes into standby after transmission 
  instance->ClearIRQFlags();
  instance->NonceTX++;
  TXdoneCallback1();
  TXdoneCallback2();
  TXdoneCallback3();
  TXdoneCallback4();
  instance->TXdoneMicros = micros();
}

void ICACHE_RAM_ATTR SX127xDriver::TXnb(uint8_t volatile *data, uint8_t length)
{
  if (instance->currOpmode == SX127x_OPMODE_TX)
  {
    Serial.println("abort TX");
    return; // we were already TXing so abort
  }
  hal.TXenable();
  instance->TXstartMicros = micros();
  instance->HeadRoom = instance->TXstartMicros - instance->TXdoneMicros;

  instance->IRQneedsClear = true;
  instance->ClearIRQFlags();
  instance->SetMode(SX127x_OPMODE_STANDBY);

  hal.writeRegister(SX127X_REG_FIFO_ADDR_PTR, SX127X_FIFO_TX_BASE_ADDR_MAX);
  hal.writeRegisterFIFO(data, length);

  instance->SetMode(SX127x_OPMODE_TX);
}

///////////////////////////////////RX Functions Non-Blocking///////////////////////////////////////////

void ICACHE_RAM_ATTR SX127xDriver::RXnbISR()
{
  hal.TXRXdisable();
  instance->IRQneedsClear = true;
  instance->ClearIRQFlags();
  hal.readRegisterFIFO(instance->RXdataBuffer, instance->RXbuffLen);
  instance->LastPacketRSSI = instance->GetLastPacketRSSI();
  instance->LastPacketSNR = instance->GetLastPacketSNR();
  instance->NonceRX++;
  RXdoneCallback1();
  RXdoneCallback2();
}

void ICACHE_RAM_ATTR SX127xDriver::RXnb()
{
  // if (instance->currOpmode == SX127x_OPMODE_RXCONTINUOUS)
  // {
  //   Serial.println("abort RX");
  //   return; // we were already TXing so abort
  // }
  instance->IRQneedsClear = true;
  instance->ClearIRQFlags();
  hal.RXenable();
  instance->SetMode(SX127x_OPMODE_STANDBY);
  hal.writeRegister(SX127X_REG_FIFO_ADDR_PTR, SX127X_FIFO_RX_BASE_ADDR_MAX);
  instance->SetMode(SX127x_OPMODE_RXCONTINUOUS);
}

// uint8_t SX127xDriver::RunCAD() TODO
// {
//   SetMode(SX127X_STANDBY);

//   hal.setRegValue(SX127X_REG_DIO_MAPPING_1, SX127X_DIO0_CAD_DONE | SX127X_DIO1_CAD_DETECTED, 7, 4);

//   SetMode(SX127X_CAD);
//   ClearIRQFlags();

//   uint32_t startTime = millis();

//   while (!digitalRead(SX127x_dio0))
//   {
//     if (millis() > (startTime + 500))
//     {
//       return (CHANNEL_FREE);
//     }
//     else
//     {
//       //yield();
//       if (digitalRead(SX127x_dio1))
//       {
//         ClearIRQFlags();
//         return (PREAMBLE_DETECTED);
//       }
//     }
//   }

//   ClearIRQFlags();
//   return (CHANNEL_FREE);
// }

void ICACHE_RAM_ATTR SX127xDriver::SetMode(SX127x_RadioOPmodes mode)
{ //if radio is not already in the required mode set it to the requested mod
  if (!(currOpmode == mode))
  {
    hal.writeRegister(ModFSKorLoRa | SX127X_REG_OP_MODE, mode);
    currOpmode = mode;
  }
}

void SX127xDriver::Config(SX127x_Bandwidth bw, SX127x_SpreadingFactor sf, SX127x_CodingRate cr, uint8_t preambleLen)
{
  Config(bw, sf, cr, currFreq, preambleLen, currSyncWord);
}

void SX127xDriver::Config(SX127x_Bandwidth bw, SX127x_SpreadingFactor sf, SX127x_CodingRate cr, uint32_t freq, uint8_t preambleLen, uint8_t syncWord)
{
  SetFrequency(freq);
  SetPreambleLength(preambleLen);
  SetOutputPower(currPWR);
  SetSpreadingFactor(sf);
  SetBandwidthCodingRate(bw, cr);
}

uint32_t ICACHE_RAM_ATTR SX127xDriver::GetCurrBandwidth()
{
  switch (currBW)
  {
  case SX127x_BW_7_80_KHZ:
    return 7.8E3;
  case SX127x_BW_10_40_KHZ:
    return 10.4E3;
  case SX127x_BW_15_60_KHZ:
    return 15.6E3;
  case SX127x_BW_20_80_KHZ:
    return 20.8E3;
  case SX127x_BW_31_25_KHZ:
    return 31.25E3;
  case SX127x_BW_41_70_KHZ:
    return 41.7E3;
  case SX127x_BW_62_50_KHZ:
    return 62.5E3;
  case SX127x_BW_125_00_KHZ:
    return 125E3;
  case SX127x_BW_250_00_KHZ:
    return 250E3;
  case SX127x_BW_500_00_KHZ:
    return 500E3;
  }
  return -1;
}

uint32_t ICACHE_RAM_ATTR SX127xDriver::GetCurrBandwidthNormalisedShifted() // this is basically just used for speedier calc of the freq offset, pre compiled for 32mhz xtal
{

  switch (currBW)
  {
  case SX127x_BW_7_80_KHZ:
    return 1026;
  case SX127x_BW_10_40_KHZ:
    return 769;
  case SX127x_BW_15_60_KHZ:
    return 513;
  case SX127x_BW_20_80_KHZ:
    return 385;
  case SX127x_BW_31_25_KHZ:
    return 256;
  case SX127x_BW_41_70_KHZ:
    return 192;
  case SX127x_BW_62_50_KHZ:
    return 128;
  case SX127x_BW_125_00_KHZ:
    return 64;
  case SX127x_BW_250_00_KHZ:
    return 32;
  case SX127x_BW_500_00_KHZ:
    return 16;
  }
  return -1;
}

void ICACHE_RAM_ATTR SX127xDriver::SetPPMoffsetReg(int32_t offset)
{
  int32_t offsetValue = ((int32_t)243) * (offset << 8) / ((((int32_t)SX127xDriver::currFreq / 1000000)) << 8);
  offsetValue >>= 8;

  uint8_t regValue = offsetValue & 0b01111111;

  if (offsetValue < 0)
  {
    regValue = regValue | 0b10000000; //set neg bit for 2s complement
  }

  hal.writeRegister(SX127x_PPMOFFSET, regValue);
}

bool ICACHE_RAM_ATTR SX127xDriver::GetFrequencyErrorbool()
{
  return (hal.readRegister(SX127X_REG_FEI_MSB) & 0b1000) >> 3; // returns true if pos freq error, neg if false
}

int32_t ICACHE_RAM_ATTR SX127xDriver::GetFrequencyError()
{

  WORD_ALIGNED_ATTR uint8_t reg[3] = {0x0, 0x0, 0x0};
  hal.readRegisterBurst(SX127X_REG_FEI_MSB, sizeof(reg), reg);

  uint32_t RegFei = ((reg[0] & 0b0111) << 16) + (reg[1] << 8) + reg[2];

  int32_t intFreqError = RegFei;

  if ((reg[0] & 0b1000) >> 3)
  {
    intFreqError -= 524288; // Sign bit is on
  }

  int32_t fErrorHZ = (intFreqError >> 3) * (SX127xDriver::GetCurrBandwidthNormalisedShifted()); // bit shift hackery so we don't have to use floaty bois; the >> 3 is intentional and is a simplification of the formula on page 114 of sx1276 datasheet
  fErrorHZ >>= 4;

  return fErrorHZ;
}

uint8_t ICACHE_RAM_ATTR SX127xDriver::UnsignedGetLastPacketRSSI()
{
  return (hal.getRegValue(SX127X_REG_PKT_RSSI_VALUE));
}

int8_t ICACHE_RAM_ATTR SX127xDriver::GetLastPacketRSSI()
{
  return (-157 + hal.getRegValue(SX127X_REG_PKT_RSSI_VALUE));
}

int8_t ICACHE_RAM_ATTR SX127xDriver::GetCurrRSSI()
{
  return (-157 + hal.getRegValue(SX127X_REG_RSSI_VALUE));
}

int8_t ICACHE_RAM_ATTR SX127xDriver::GetLastPacketSNR()
{
  int8_t rawSNR = (int8_t)hal.getRegValue(SX127X_REG_PKT_SNR_VALUE);
  return (rawSNR / 4.0);
}

void ICACHE_RAM_ATTR SX127xDriver::ClearIRQFlags()
{
  if (IRQneedsClear)
  {
    hal.writeRegister(SX127X_REG_IRQ_FLAGS, 0b11111111);
    IRQneedsClear = false;
  }
}
