#include <Arduino.h>
#include "LoRa_SX127x.h"
#include "LoRa_SX1278.h"
#include "LoRa_lowlevel.h"

uint8_t SX1278rxCont()
{ //ADDED CHANGED
  // get header mode
  bool headerExplMode = false;
  if (getRegValue(SX127X_REG_MODEM_CONFIG_1, 0, 0) == SX1278_HEADER_EXPL_MODE)
  {
    headerExplMode = true;
  }

  // execute common part
  //return
  SX127xDriver::RXnb();
  return (ERR_NONE);
}

uint8_t SX1278rxSingle(uint8_t *data, uint8_t length)
{
  // get header mode
  bool headerExplMode = false;
  if (getRegValue(SX127X_REG_MODEM_CONFIG_1, 0, 0) == SX1278_HEADER_EXPL_MODE)
  {
    headerExplMode = true;
  }

  // execute common part
  return SX127xDriver::RXsingle(data, length);
}

// uint8_t SX1278setBandwidth(Bandwidth bw) {  //moved renamed
// uint8_t state = config(bw, _sf, _cr, _freq, _syncWord);
// if (state == ERR_NONE) {
// _bw = bw;
// }
// return (state);
// }

// uint8_t SX1278setSpreadingFactor(SpreadingFactor sf) {  //moved renamed
// uint8_t state = config(_bw, sf, _cr, _freq, _syncWord);
// if (state == ERR_NONE) {
// _sf = sf;
// }
// return (state);
// }

// uint8_t SX1278setCodingRate(CodingRate cr) { //moved renamed
// uint8_t state = config(_bw, _sf, cr, _freq, _syncWord);
// if (state == ERR_NONE) {
// _cr = cr;
// }
// return (state);
// }

// uint8_t SX1278setFrequency(float freq) {
// uint8_t state = config(_bw, _sf, _cr, freq, _syncWord);
// if (state == ERR_NONE) {
// _freq = freq;
// }
// return (state);
// }

// uint8_t SX1278setSyncWord(uint8_t syncWord) {
// uint8_t state = config(_bw, _sf, _cr, _freq, syncWord);
// if (state == ERR_NONE) {
// _syncWord = syncWord;
// }
// return (state);
// }

uint8_t SX1278config(Bandwidth bw, SpreadingFactor sf, CodingRate cr, uint32_t freq, uint8_t syncWord)
{
  uint8_t status = ERR_NONE;
  uint8_t newBandwidth, newSpreadingFactor, newCodingRate;

  // check the supplied BW, CR and SF values
  switch (bw)
  {
  case BW_7_80_KHZ:
    newBandwidth = SX1278_BW_7_80_KHZ;
    break;
  case BW_10_40_KHZ:
    newBandwidth = SX1278_BW_10_40_KHZ;
    break;
  case BW_15_60_KHZ:
    newBandwidth = SX1278_BW_15_60_KHZ;
    break;
  case BW_20_80_KHZ:
    newBandwidth = SX1278_BW_20_80_KHZ;
    break;
  case BW_31_25_KHZ:
    newBandwidth = SX1278_BW_31_25_KHZ;
    break;
  case BW_41_70_KHZ:
    newBandwidth = SX1278_BW_41_70_KHZ;
    break;
  case BW_62_50_KHZ:
    newBandwidth = SX1278_BW_62_50_KHZ;
    break;
  case BW_125_00_KHZ:
    newBandwidth = SX1278_BW_125_00_KHZ;
    break;
  case BW_250_00_KHZ:
    newBandwidth = SX1278_BW_250_00_KHZ;
    break;
  case BW_500_00_KHZ:
    newBandwidth = SX1278_BW_500_00_KHZ;
    break;
  default:
    return (ERR_INVALID_BANDWIDTH);
  }

  switch (sf)
  {
  case SF_6:
    newSpreadingFactor = SX127X_SF_6;
    break;
  case SF_7:
    newSpreadingFactor = SX127X_SF_7;
    break;
  case SF_8:
    newSpreadingFactor = SX127X_SF_8;
    break;
  case SF_9:
    newSpreadingFactor = SX127X_SF_9;
    break;
  case SF_10:
    newSpreadingFactor = SX127X_SF_10;
    break;
  case SF_11:
    newSpreadingFactor = SX127X_SF_11;
    break;
  case SF_12:
    newSpreadingFactor = SX127X_SF_12;
    break;
  default:
    return (ERR_INVALID_SPREADING_FACTOR);
  }

  switch (cr)
  {
  case CR_4_5:
    newCodingRate = SX1278_CR_4_5;
    break;
  case CR_4_6:
    newCodingRate = SX1278_CR_4_6;
    break;
  case CR_4_7:
    newCodingRate = SX1278_CR_4_7;
    break;
  case CR_4_8:
    newCodingRate = SX1278_CR_4_8;
    break;
  default:
    return (ERR_INVALID_CODING_RATE);
  }

  if ((freq < 137000000) || (freq > 525000000))
  {
    Serial.print("Invalid Frequnecy!: ");
    Serial.println(freq);
    return (ERR_INVALID_FREQUENCY);
  }

  // execute common part
  status = SX1278configCommon(newBandwidth, newSpreadingFactor, newCodingRate, freq, syncWord);
  if (status != ERR_NONE)
  {
    return (status);
  }

  // configuration successful, save the new settings
  SX127xDriver::currBW = bw;
  SX127xDriver::currSF = sf;
  SX127xDriver::currCR = cr;
  SX127xDriver::currFreq = freq;

  return (ERR_NONE);
}

uint8_t SX1278configCommon(uint8_t bw, uint8_t sf, uint8_t cr, uint32_t freq, uint8_t syncWord)
{
  // configure common registers
  uint8_t status = SX127xDriver::SX127xConfig(bw, sf, cr, freq, syncWord);
  if (status != ERR_NONE)
  {
    return (status);
  }

  // output power configuration
  //status = setRegValue(SX127X_REG_PA_CONFIG, SX1278_MAX_POWER, 6, 4); // changed

  status = setRegValue(SX1278_REG_PA_DAC, SX127X_PA_BOOST_OFF, 2, 0);
  if (status != ERR_NONE)
  {
    return (status);
  }

  status = setRegValue(SX1278_REG_MODEM_CONFIG_3, SX1278_AGC_AUTO_ON, 2, 2);

  // enable LNA gain setting by register and set low datarate optimizations for SF11/SF12 with 125 kHz bandwidth
  //status = setRegValue(SX1278_REG_MODEM_CONFIG_3, SX1278_AGC_AUTO_OFF, 2, 2);

  if ((bw == SX1278_BW_125_00_KHZ) && ((sf == SX127X_SF_11) || (sf == SX127X_SF_12)))
  {
    status = setRegValue(SX1278_REG_MODEM_CONFIG_3, SX1278_LOW_DATA_RATE_OPT_ON, 0, 0);
  }
  else
  {
    status = setRegValue(SX1278_REG_MODEM_CONFIG_3, SX1278_LOW_DATA_RATE_OPT_OFF, 0, 0);
  }
  if (status != ERR_NONE)
  {
    return (status);
  }

  // set SF6 optimizations
  if (sf == SX127X_SF_6)
  {
    status = setRegValue(SX127X_REG_MODEM_CONFIG_2, SX1278_RX_CRC_MODE_OFF, 2, 2);
    //status = setRegValue(SX127X_REG_MODEM_CONFIG_2, SX1278_RX_CRC_MODE_ON, 2, 2);
    status = setRegValue(SX127X_REG_MODEM_CONFIG_1, bw | cr | SX1278_HEADER_IMPL_MODE);
  }
  else
  {
    //status = setRegValue(SX127X_REG_MODEM_CONFIG_2, SX1278_RX_CRC_MODE_ON, 2, 2);
    status = setRegValue(SX127X_REG_MODEM_CONFIG_2, SX1278_RX_CRC_MODE_OFF, 2, 2);
    //status = setRegValue(SX127X_REG_MODEM_CONFIG_1, bw | cr | SX1278_HEADER_EXPL_MODE);
    status = setRegValue(SX127X_REG_MODEM_CONFIG_1, bw | cr | SX1278_HEADER_IMPL_MODE);
  }

  if (bw == SX1278_BW_500_00_KHZ)
  {
    //datasheet errata reconmendation http://caxapa.ru/thumbs/972894/SX1276_77_8_ErrataNote_1.1_STD.pdf
    status = setRegValue(0x36, 0x02);
    status = setRegValue(0x3a, 0x64);
  }
  else
  {
    status = setRegValue(0x36, 0x03);
  }

  return (status);
}

uint8_t SX1278begin(uint8_t nss, uint8_t dio0, uint8_t dio1)
{
  // initialize low-level drivers
  //initModule(nss, dio0, dio1);
  Serial.println("Init module SX1278");
  initModule(nss, dio0, dio1);

  // execute common part
  uint8_t status = SX127xDriver::SX127xBegin();
  if (status != ERR_NONE)
  {
    return (status);
  }

  // start configuration
  return (SX1278config(SX127xDriver::currBW, SX127xDriver::currSF, SX127xDriver::currCR, SX127xDriver::currFreq, SX127xDriver::_syncWord));
}
