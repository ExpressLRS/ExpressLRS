#include "SX127x.h"
#include "logging.h"
#include "RFAMP_hal.h"

SX127xHal hal;
SX127xDriver *SX127xDriver::instance = NULL;

RFAMP_hal RFAMP;

#ifdef USE_SX1276_RFO_HF
  #ifndef OPT_USE_SX1276_RFO_HF
    #define OPT_USE_SX1276_RFO_HF true
  #endif
#else
  #define OPT_USE_SX1276_RFO_HF false
#endif

const uint8_t SX127x_AllowedSyncwords[105] =
    {0, 5, 6, 7, 11, 12, 13, 15, 18,
     21, 23, 26, 29, 30, 31, 33, 34,
     37, 38, 39, 40, 42, 44, 50, 51,
     54, 55, 57, 58, 59, 61, 63, 65,
     67, 68, 71, 77, 78, 79, 80, 82,
     84, 86, 89, 92, 94, 96, 97, 99,
     101, 102, 105, 106, 109, 111, 113, 115,
     117, 118, 119, 121, 122, 124, 126, 127,
     129, 130, 138, 143, 161, 170, 172, 173,
     175, 180, 181, 182, 187, 190, 191, 192,
     193, 196, 199, 201, 204, 205, 208, 209,
     212, 213, 219, 220, 221, 223, 227, 229,
     235, 239, 240, 242, 243, 246, 247, 255};

//////////////////////////////////////////////

SX127xDriver::SX127xDriver(): SX12xxDriverCommon()
{
  instance = this;
  // default values from datasheet
  currSyncWord = SX127X_SYNC_WORD;
  currBW =SX127x_BW_125_00_KHZ;
  currSF = SX127x_SF_7;
  currCR = SX127x_CR_4_5;
  currOpmode = SX127x_OPMODE_SLEEP;
  ModFSKorLoRa = SX127x_OPMODE_LORA;
  // Dummy default values which are overwritten during setup
  currPreambleLen = 0;
  PayloadLength = 8;
  currFreq = 0;
  headerExplMode = false;
  crcEnabled = false;
  lowFrequencyMode = SX1278_HIGH_FREQ;
}

bool SX127xDriver::Begin()
{
  hal.init();
  hal.IsrCallback_1 = &SX127xDriver::IsrCallback_1;
  hal.IsrCallback_2 = &SX127xDriver::IsrCallback_2;

  hal.reset();
  DBGLN("SX127x Begin");

  RFAMP.init();

  // currFreq must be set before calling Radio.Begin so that lowFrequencyMode can be set correctly.
  if (currFreq < (uint32_t)((double)525000000 / (double)FREQ_STEP))
  {
    lowFrequencyMode = SX1278_LOW_FREQ;
    DBGLN("Setting 'lowFrequencyMode' used for 433MHz.");
  }
  
  SetMode(SX127x_OPMODE_STANDBY, SX12XX_Radio_All);

  if (!DetectChip(SX12XX_Radio_1))
  {
    return false;
  }

  if (GPIO_PIN_NSS_2 != UNDEF_PIN)
  {
    if (!DetectChip(SX12XX_Radio_2))
    {
      return false;
    }
  }

  ConfigLoraDefaults();
  // Force the next power update
  pwrCurrent = PWRPENDING_NONE;
  SetOutputPower(0);
  CommitOutputPower();

  return true;
}

void SX127xDriver::End()
{
  SetMode(SX127x_OPMODE_SLEEP, SX12XX_Radio_All);
  hal.end();
  RFAMP.TXRXdisable();
  RemoveCallbacks();
}

void SX127xDriver::ConfigLoraDefaults()
{
  hal.writeRegister(SX127X_REG_OP_MODE, SX127x_OPMODE_SLEEP, SX12XX_Radio_All);
  hal.writeRegister(SX127X_REG_OP_MODE, ModFSKorLoRa, SX12XX_Radio_All); //must be written in sleep mode
  SetMode(SX127x_OPMODE_STANDBY, SX12XX_Radio_All);

  hal.writeRegister(SX127X_REG_PAYLOAD_LENGTH, PayloadLength, SX12XX_Radio_All);
  SetSyncWord(currSyncWord);
  hal.writeRegister(SX127X_REG_FIFO_TX_BASE_ADDR, SX127X_FIFO_TX_BASE_ADDR_MAX, SX12XX_Radio_All);
  hal.writeRegister(SX127X_REG_FIFO_RX_BASE_ADDR, SX127X_FIFO_RX_BASE_ADDR_MAX, SX12XX_Radio_All);
  hal.writeRegisterBits(SX127X_REG_DIO_MAPPING_1, SX127X_DIO0_RXTX_DONE, SX127X_DIO0_MASK, SX12XX_Radio_All); //undocumented "hack", looking at Table 18 from datasheet SX127X_REG_DIO_MAPPING_1 = 11 appears to be unspported by infact it generates an intterupt on both RXdone and TXdone, this saves switching modes.
  hal.writeRegister(SX127X_REG_LNA, SX127X_LNA_BOOST_ON, SX12XX_Radio_All);
  hal.writeRegister(SX1278_REG_MODEM_CONFIG_3, SX1278_AGC_AUTO_ON | SX1278_LOW_DATA_RATE_OPT_OFF, SX12XX_Radio_All);
  hal.writeRegisterBits(SX127X_REG_OCP, SX127X_OCP_ON | SX127X_OCP_150MA, SX127X_OCP_MASK, SX12XX_Radio_All); //150ma max current
  SetPreambleLength(SX127X_PREAMBLE_LENGTH_LSB);
}

void SX127xDriver::SetBandwidthCodingRate(SX127x_Bandwidth bw, SX127x_CodingRate cr)
{
  if ((currBW != bw) || (currCR != cr))
  {
    if (currSF == SX127x_SF_6) // set SF6 optimizations
    {
      hal.writeRegister(SX127X_REG_MODEM_CONFIG_1, bw | cr | SX127x_HEADER_IMPL_MODE, SX12XX_Radio_All);
      SetCRCMode(false);
    }
    else
    {
      if (headerExplMode)
      {
        hal.writeRegister(SX127X_REG_MODEM_CONFIG_1, bw | cr | SX127x_HEADER_EXPL_MODE, SX12XX_Radio_All);
      }
      else
      {
        hal.writeRegister(SX127X_REG_MODEM_CONFIG_1, bw | cr | SX127x_HEADER_IMPL_MODE, SX12XX_Radio_All);
      }
      SetCRCMode(crcEnabled);
    }

    #if !defined(RADIO_SX1272) //does not apply to SX1272
      if (bw == SX127x_BW_500_00_KHZ)
      {
        //data-sheet errata recommendation http://caxapa.ru/thumbs/972894/SX1276_77_8_ErrataNote_1.1_STD.pdf
        hal.writeRegister(0x36, 0x02, SX12XX_Radio_All);
        hal.writeRegister(0x3a, lowFrequencyMode ? 0x7F : 0x64, SX12XX_Radio_All);
      }
      else
      {
        hal.writeRegister(0x36, 0x03, SX12XX_Radio_All);
      }
    #endif

    currCR = cr;
    currBW = bw;
  }
}

void SX127xDriver::SetCRCMode(bool on)
{
  if(on)
  {
    #if defined(RADIO_SX1272)
      hal.writeRegisterBits(SX127X_REG_MODEM_CONFIG_1, SX1272_RX_CRC_MODE_ON, SX1272_RX_CRC_MODE_MASK, SX12XX_Radio_All);
    #else
      hal.writeRegisterBits(SX127X_REG_MODEM_CONFIG_2, SX1278_RX_CRC_MODE_ON, SX1278_RX_CRC_MODE_MASK, SX12XX_Radio_All);
    #endif
  }
  else
  {
    #if defined(RADIO_SX1272)
      hal.writeRegisterBits(SX127X_REG_MODEM_CONFIG_1, SX1272_RX_CRC_MODE_OFF, SX1272_RX_CRC_MODE_MASK, SX12XX_Radio_All);
    #else
      hal.writeRegisterBits(SX127X_REG_MODEM_CONFIG_2, SX1278_RX_CRC_MODE_OFF, SX1278_RX_CRC_MODE_MASK, SX12XX_Radio_All);
    #endif
  }
}

bool SyncWordOk(uint8_t syncWord)
{
  for (unsigned int i = 0; i < sizeof(SX127x_AllowedSyncwords); i++)
  {
    if (syncWord == SX127x_AllowedSyncwords[i])
    {
      return true;
    }
  }
  return false;
}

void SX127xDriver::SetSyncWord(uint8_t syncWord)
{
  uint8_t _syncWord = syncWord;

  while (SyncWordOk(_syncWord) == false)
  {
    _syncWord++;
  }

  if(syncWord != _syncWord){
    DBGLN("Using syncword: %d instead of: %d", _syncWord, syncWord);
  }

  hal.writeRegister(SX127X_REG_SYNC_WORD, _syncWord, SX12XX_Radio_All);
  currSyncWord = _syncWord;
}

/***
 * @brief: Schedule an output power change after the next transmit
 * The radio must be in SX127x_OPMODE_STANDBY to change the power
 ***/
void SX127xDriver::SetOutputPower(uint8_t Power)
{
  uint8_t pwrNew;
  if (OPT_USE_SX1276_RFO_HF)
  {
    pwrNew = SX127X_PA_SELECT_RFO | SX127X_MAX_OUTPUT_POWER_RFO_HF | Power;
  }
  else
  {
    pwrNew = SX127X_PA_SELECT_BOOST | SX127X_MAX_OUTPUT_POWER | Power;
  }

  if ((pwrPending == PWRPENDING_NONE && pwrCurrent != pwrNew) || pwrPending != pwrNew)
  {
    pwrPending = pwrNew;
  }
}

void ICACHE_RAM_ATTR SX127xDriver::CommitOutputPower()
{
  if (pwrPending == PWRPENDING_NONE)
    return;

  pwrCurrent = pwrPending;
  pwrPending = PWRPENDING_NONE;
  hal.writeRegister(SX127X_REG_PA_CONFIG, pwrCurrent, SX12XX_Radio_All);
}

void SX127xDriver::SetPreambleLength(uint8_t PreambleLen)
{
  if (currPreambleLen != PreambleLen)
  {
    hal.writeRegister(SX127X_REG_PREAMBLE_LSB, PreambleLen, SX12XX_Radio_All);
    currPreambleLen = PreambleLen;
  }
}

void SX127xDriver::SetSpreadingFactor(SX127x_SpreadingFactor sf)
{
  if (currSF != sf)
  {
    hal.writeRegisterBits(SX127X_REG_MODEM_CONFIG_2, sf | SX127X_TX_MODE_SINGLE,  SX127X_SPREADING_FACTOR_MASK | SX127X_TX_MODE_MASK, SX12XX_Radio_All);
    if (sf == SX127x_SF_6)
    {
      hal.writeRegisterBits(SX127X_REG_DETECT_OPTIMIZE, SX127X_DETECT_OPTIMIZE_SF_6, SX127X_DETECT_OPTIMIZE_SF_MASK, SX12XX_Radio_All);
      hal.writeRegister(SX127X_REG_DETECTION_THRESHOLD, SX127X_DETECTION_THRESHOLD_SF_6, SX12XX_Radio_All);
    }
    else
    {
      hal.writeRegisterBits(SX127X_REG_DETECT_OPTIMIZE, SX127X_DETECT_OPTIMIZE_SF_7_12, SX127X_DETECT_OPTIMIZE_SF_MASK, SX12XX_Radio_All);
      hal.writeRegister(SX127X_REG_DETECTION_THRESHOLD, SX127X_DETECTION_THRESHOLD_SF_7_12, SX12XX_Radio_All);
    }
    currSF = sf;
  }
}

void ICACHE_RAM_ATTR SX127xDriver::SetFrequencyHz(uint32_t freq, SX12XX_Radio_Number_t radioNumber)
{  
  int32_t regfreq = ((uint32_t)((double)freq / (double)FREQ_STEP));

  SetFrequencyReg(regfreq, radioNumber);
}

void ICACHE_RAM_ATTR SX127xDriver::SetFrequencyReg(uint32_t regfreq, SX12XX_Radio_Number_t radioNumber)
{
  currFreq = regfreq;
  SetMode(SX127x_OPMODE_STANDBY, radioNumber);

  uint8_t FRQ_MSB = (uint8_t)((regfreq >> 16) & 0xFF);
  uint8_t FRQ_MID = (uint8_t)((regfreq >> 8) & 0xFF);
  uint8_t FRQ_LSB = (uint8_t)(regfreq & 0xFF);

  WORD_ALIGNED_ATTR uint8_t outbuff[3] = {FRQ_MSB, FRQ_MID, FRQ_LSB}; //check speedup

  hal.writeRegister(SX127X_REG_FRF_MSB, outbuff, sizeof(outbuff), radioNumber);
}

void ICACHE_RAM_ATTR SX127xDriver::SetRxTimeoutUs(uint32_t interval)
{
  timeoutSymbols = 0; // no timeout i.e. use continuous mode
  if (interval)
  {
    unsigned int spread = 0;
    switch (currSF)
    {
    case SX127x_SF_6:
      spread = 6;
      break;
    case SX127x_SF_7:
      spread = 7;
      break;
    case SX127x_SF_8:
      spread = 8;
      break;
    case SX127x_SF_9:
      spread = 9;
      break;
    case SX127x_SF_10:
      spread = 10;
      break;
    case SX127x_SF_11:
      spread = 11;
      break;
    case SX127x_SF_12:
      spread = 12;
      break;
    }
    uint32_t symbolTimeUs = ((uint32_t)(1 << spread)) * 1000000 / GetCurrBandwidth();
    timeoutSymbols = interval / symbolTimeUs;
    hal.writeRegisterBits(SX127X_REG_SYMB_TIMEOUT_MSB, timeoutSymbols >> 8, SX127X_REG_SYMB_TIMEOUT_MSB_MASK, SX12XX_Radio_All);  // set the timeout MSB
    hal.writeRegister(SX127X_REG_SYMB_TIMEOUT_LSB, timeoutSymbols & 0xFF, SX12XX_Radio_All);
    DBGLN("SetRxTimeout(%u), symbolTime=%uus symbols=%u", interval, (uint32_t)symbolTimeUs, timeoutSymbols)
  }
}

bool SX127xDriver::DetectChip(SX12XX_Radio_Number_t radioNumber)
{
  DBG("Detecting radio %x... ", radioNumber);
  uint8_t i = 0;
  bool flagFound = false;
  while ((i < 3) && !flagFound)
  {
    uint8_t version = hal.readRegister(SX127X_REG_VERSION, radioNumber);
    DBG("%x", version);
    if (version == SX127X_VERSION)
    {
      flagFound = true;
    }
    else
    {
      DBGLN(" not found! (%d of 3 tries) REG_VERSION == 0x%x", i+1, version);
      delay(200);
      i++;
    }
  }

  if (!flagFound)
  {
    DBGLN(" not found!");
    return false;
  }
  else
  {
    DBGLN(" found! (match by REG_VERSION == 0x%x", SX127X_VERSION);
  }
  return true;
}

/////////////////////////////////////TX functions/////////////////////////////////////////

void ICACHE_RAM_ATTR SX127xDriver::TXnbISR()
{
  currOpmode = SX127x_OPMODE_STANDBY; //goes into standby after transmission
  //TXdoneMicros = micros();
  // The power level must be changed when in SX127x_OPMODE_STANDBY, so this lags power
  // changes by at most 1 packet, but does not interrupt any pending RX/TX
  CommitOutputPower();
  TXdoneCallback();
}

void ICACHE_RAM_ATTR SX127xDriver::TXnb(uint8_t * data, uint8_t size, SX12XX_Radio_Number_t radioNumber)
{
  // if (currOpmode == SX127x_OPMODE_TX)
  // {
  //   DBGLN("abort TX");
  //   return; // we were already TXing so abort. this should never happen!!!
  // }
  SetMode(SX127x_OPMODE_STANDBY, SX12XX_Radio_All);

  if (radioNumber == SX12XX_Radio_Default)
  {
      radioNumber = lastSuccessfulPacketRadio;
  }
  
  RFAMP.TXenable(radioNumber);
  hal.writeRegister(SX127X_REG_FIFO_ADDR_PTR, SX127X_FIFO_TX_BASE_ADDR_MAX, radioNumber);
  hal.writeRegister(SX127X_REG_FIFO, data, size, radioNumber);

  SetMode(SX127x_OPMODE_TX, radioNumber);
}

///////////////////////////////////RX Functions Non-Blocking///////////////////////////////////////////

bool ICACHE_RAM_ATTR SX127xDriver::RXnbISR(SX12XX_Radio_Number_t radioNumber)
{
  uint8_t const FIFOaddr = hal.readRegister(SX127X_REG_FIFO_RX_CURRENT_ADDR, radioNumber);
  hal.writeRegister(SX127X_REG_FIFO_ADDR_PTR, FIFOaddr, radioNumber);
  hal.readRegister(SX127X_REG_FIFO, RXdataBuffer, PayloadLength, radioNumber);

  if (timeoutSymbols)
  {
    // From page 42 of the datasheet rev 7
    // In Rx Single mode, the device will return to Standby mode as soon as the interrupt occurs
    currOpmode = SX127x_OPMODE_STANDBY;
  }

  return RXdoneCallback(SX12XX_RX_OK);
}

void ICACHE_RAM_ATTR SX127xDriver::RXnb()
{
  RFAMP.RXenable();
  
  if (timeoutSymbols)
  {
    SetMode(SX127x_OPMODE_RXSINGLE, SX12XX_Radio_All);
  }
  else
  {
    SetMode(SX127x_OPMODE_RXCONTINUOUS, SX12XX_Radio_All);
  }
}

void ICACHE_RAM_ATTR SX127xDriver::GetLastPacketStats()
{
  LastPacketRSSI = GetLastPacketRSSI();
  LastPacketSNRRaw = GetLastPacketSNRRaw();
  // https://www.mouser.com/datasheet/2/761/sx1276-1278113.pdf
  // Section 3.5.5 (page 87)
  int8_t negOffset = (LastPacketSNRRaw < 0) ? (LastPacketSNRRaw / RADIO_SNR_SCALE) : 0;
  LastPacketRSSI += negOffset;
}

void ICACHE_RAM_ATTR SX127xDriver::SetMode(SX127x_RadioOPmodes mode, SX12XX_Radio_Number_t radioNumber)
{
  /*
  Comment out since it is difficult to keep track of dual radios.
  When checking SPI it is also useful to see every possible SPI transaction to make sure it fits when required.
  */
  // if (mode == currOpmode)
  // {
  //    return;
  // }
  
  hal.writeRegister(SX127X_REG_OP_MODE, mode | lowFrequencyMode, radioNumber);
  currOpmode = mode;
}

void SX127xDriver::Config(uint8_t bw, uint8_t sf, uint8_t cr, uint32_t freq, uint8_t preambleLen, bool InvertIQ, uint8_t _PayloadLength, uint32_t interval)
{
  Config(bw, sf, cr, freq, preambleLen, currSyncWord, InvertIQ, _PayloadLength, interval);
}

void SX127xDriver::Config(uint8_t bw, uint8_t sf, uint8_t cr, uint32_t freq, uint8_t preambleLen, uint8_t syncWord, bool InvertIQ, uint8_t _PayloadLength, uint32_t interval)
{
  PayloadLength = _PayloadLength;
  ConfigLoraDefaults();
  SetPreambleLength(preambleLen);
  SetSpreadingFactor((SX127x_SpreadingFactor)sf);
  SetBandwidthCodingRate((SX127x_Bandwidth)bw, (SX127x_CodingRate)cr);
  SetFrequencyReg(freq);
  SetRxTimeoutUs(interval);
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

/**
 * Set the PPMcorrection register to adjust data rate to frequency error
 * @param offset is in Hz or FREQ_STEP (FREQ_HZ_TO_REG_VAL) units, whichever
 *    was used to SetFrequencyHz/SetFrequencyReg
 */
void ICACHE_RAM_ATTR SX127xDriver::SetPPMoffsetReg(int32_t offset)
{
  int8_t offsetPPM = (offset * 1e6 / currFreq) * 95 / 100;
  hal.writeRegister(SX127x_PPMOFFSET, (uint8_t)offsetPPM, processingPacketRadio);
}

bool ICACHE_RAM_ATTR SX127xDriver::GetFrequencyErrorbool()
{
  return (hal.readRegister(SX127X_REG_FEI_MSB, processingPacketRadio) & 0b1000) >> 3; // returns true if pos freq error, neg if false
}

int32_t ICACHE_RAM_ATTR SX127xDriver::GetFrequencyError()
{

  WORD_ALIGNED_ATTR uint8_t reg[3] = {0x0, 0x0, 0x0};
  hal.readRegister(SX127X_REG_FEI_MSB, reg, sizeof(reg), processingPacketRadio);

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

uint8_t ICACHE_RAM_ATTR SX127xDriver::UnsignedGetLastPacketRSSI(SX12XX_Radio_Number_t radioNumber)
{
  return hal.readRegister(SX127X_REG_PKT_RSSI_VALUE, radioNumber);
}

int8_t ICACHE_RAM_ATTR SX127xDriver::GetLastPacketRSSI()
{
  return ((lowFrequencyMode ? -164 : -157) + hal.readRegister(SX127X_REG_PKT_RSSI_VALUE, processingPacketRadio));
}

int8_t ICACHE_RAM_ATTR SX127xDriver::GetCurrRSSI(SX12XX_Radio_Number_t radioNumber)
{
  return ((lowFrequencyMode ? -164 : -157) + hal.readRegister(SX127X_REG_RSSI_VALUE, radioNumber));
}

int8_t ICACHE_RAM_ATTR SX127xDriver::GetLastPacketSNRRaw()
{
  return (int8_t)hal.readRegister(SX127X_REG_PKT_SNR_VALUE, processingPacketRadio);
}

uint8_t ICACHE_RAM_ATTR SX127xDriver::GetIrqFlags(SX12XX_Radio_Number_t radioNumber)
{
  return hal.readRegister(SX127X_REG_IRQ_FLAGS, radioNumber);
}

void ICACHE_RAM_ATTR SX127xDriver::ClearIrqFlags(SX12XX_Radio_Number_t radioNumber)
{
  hal.writeRegister(SX127X_REG_IRQ_FLAGS, SX127X_CLEAR_IRQ_FLAG_ALL, radioNumber);
}

void ICACHE_RAM_ATTR SX127xDriver::IsrCallback_1()
{
    instance->IsrCallback(SX12XX_Radio_1);
}

void ICACHE_RAM_ATTR SX127xDriver::IsrCallback_2()
{
    instance->IsrCallback(SX12XX_Radio_2);
}

void ICACHE_RAM_ATTR SX127xDriver::IsrCallback(SX12XX_Radio_Number_t radioNumber)
{
    instance->processingPacketRadio = radioNumber;
    SX12XX_Radio_Number_t irqClearRadio = radioNumber;

    uint8_t irqStatus = instance->GetIrqFlags(radioNumber);
    if (irqStatus & SX127X_CLEAR_IRQ_FLAG_TX_DONE)
    {
        RFAMP.TXRXdisable();
        instance->TXnbISR();
        irqClearRadio = SX12XX_Radio_All;
    }
    else if (irqStatus & SX127X_CLEAR_IRQ_FLAG_RX_DONE)
    {
        if (instance->RXnbISR(radioNumber))
        {
            instance->lastSuccessfulPacketRadio = radioNumber;
            irqClearRadio = SX12XX_Radio_All;
        }

    }
    else if (irqStatus == SX127X_CLEAR_IRQ_FLAG_NONE)
    {
        return;
    }
    
    instance->ClearIrqFlags(irqClearRadio);
}

// int16_t MeasureNoiseFloor() TODO disabled for now
// {
//     int NUM_READS = RSSI_FLOOR_NUM_READS * FHSSgetChannelCount();
//     float returnval = 0;

//     for (uint32_t freq = 0; freq < FHSSgetChannelCount(); freq++)
//     {
//         FHSSsetCurrIndex(freq);
//         Radio.SetMode(SX127X_CAD);

//         for (int i = 0; i < RSSI_FLOOR_NUM_READS; i++)
//         {
//             returnval = returnval + Radio.GetCurrRSSI();
//             delay(5);
//         }
//     }
//     returnval = returnval / NUM_READS;
//     return (returnval);
// }

// uint8_t SX127xDriver::RunCAD() TODO
// {
//   SetMode(SX127X_STANDBY);

//   hal.writeRegisterBits(SX127X_REG_DIO_MAPPING_1, SX127X_DIO0_CAD_DONE | SX127X_DIO1_CAD_DETECTED, 7, 4);

//   SetMode(SX127X_CAD);
//   ClearIrqFlags();

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
//         ClearIrqFlags();
//         return (PREAMBLE_DETECTED);
//       }
//     }
//   }

//   ClearIrqFlags();
//   return (CHANNEL_FREE);
// }
