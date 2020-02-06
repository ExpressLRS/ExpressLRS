#include "LoRa_lowlevel.h"
#include "LoRa_SX127x.h"
#include "LoRa_SX1276.h"
#include "LoRa_SX1278.h"

//SX1276::SX1276(int nss, float freq, Bandwidth bw, SpreadingFactor sf, CodingRate cr, int dio0, int dio1, uint8_t syncWord) : SX1278(nss, freq, bw, sf, cr, dio0, dio1, syncWord) {
//
//}

uint8_t SX1276config(Bandwidth bw, SpreadingFactor sf, CodingRate cr, uint32_t freq, uint8_t syncWord)
{
  uint8_t status = ERR_NONE;
  uint8_t newBandwidth, newSpreadingFactor, newCodingRate;

  // check the supplied BW, CR and SF values
  switch (bw)
  {
  case BW_62_50_KHZ:
    newBandwidth = SX1276_BW_62_50_KHZ;
    break;
  case BW_125_00_KHZ:
    newBandwidth = SX1276_BW_125_00_KHZ;
    break;
  case BW_250_00_KHZ:
    newBandwidth = SX1276_BW_250_00_KHZ;
    break;
  case BW_500_00_KHZ:
    newBandwidth = SX1276_BW_500_00_KHZ;
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
    newCodingRate = SX1276_CR_4_5;
    break;
  case CR_4_6:
    newCodingRate = SX1276_CR_4_6;
    break;
  case CR_4_7:
    newCodingRate = SX1276_CR_4_7;
    break;
  case CR_4_8:
    newCodingRate = SX1276_CR_4_8;
    break;
  default:
    return (ERR_INVALID_CODING_RATE);
  }

  if ((freq < 137000000) || (freq > 1020000000))
  {
    DEBUG_PRINT("Invalid Frequnecy!: ");
    DEBUG_PRINTLN(freq);
    return (ERR_INVALID_FREQUENCY);
  }

  // execute common part
  status = SX1276configCommon(newBandwidth, newSpreadingFactor, newCodingRate, freq, syncWord);
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

// uint8_t SX1276setPower()
// {
//   uint8_t status = setRegValue(SX127X_REG_PA_CONFIG, 0b00000000, 3, 0);
//   status = setRegValue(SX1278_REG_PA_DAC, SX127X_PA_BOOST_OFF, 2, 0);
//   if (status != ERR_NONE)
//   {
//     return (status);
//   }
//   else
//   {
//     return (ERR_NONE);
//   }
// }

uint8_t SX1276configCommon(uint8_t bw, uint8_t sf, uint8_t cr, uint32_t freq, uint8_t syncWord)
{
  // configure common registers
  uint8_t status = SX127xDriver::SX127xConfig(bw, sf, cr, freq, syncWord);
  if (status != ERR_NONE)
  {
    return (status);
  }

  // output power configuration  //changed
  //   status = setRegValue(SX127X_REG_PA_CONFIG, SX1278_MAX_POWER, 6, 4);
  //    status = setRegValue(SX1278_REG_PA_DAC, SX127X_PA_BOOST_ON, 2, 0);
  //    if (status != ERR_NONE) {
  //      return (status);
  //    }

  //  status = setRegValue(SX127X_REG_PA_CONFIG, SX127X_PA_SELECT_BOOST);

  //status = setRegValue(SX127X_REG_OCP, SX127X_OCP_ON | SX127X_OCP_TRIM, 5, 0);
  status = setRegValue(SX127X_REG_OCP, SX127X_OCP_ON | 15, 5, 0);

  //status = setRegValue(SX127X_REG_LNA, SX127X_LNA_GAIN_6 | SX127X_LNA_BOOST_ON);
  //  if (status != ERR_NONE) {
  //    return (status);
  //  }

  //status = setRegValue(SX127X_REG_PA_CONFIG, 0b00000000, 3, 0);
  status = setRegValue(SX1278_REG_PA_DAC, SX127X_PA_BOOST_OFF, 2, 0);
  if (status != ERR_NONE)
  {
    return (status);
  }

  status = setRegValue(SX1278_REG_MODEM_CONFIG_3, SX1278_AGC_AUTO_ON, 2, 2);

  // enable LNA gain setting by register and set low datarate optimizations for SF11/SF12 with 125 kHz bandwidth
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
    status = setRegValue(SX127X_REG_MODEM_CONFIG_2, SX1278_RX_CRC_MODE_OFF, 2, 2);
    //status = setRegValue(SX127X_REG_MODEM_CONFIG_2, SX1278_RX_CRC_MODE_ON, 2, 2); //Changed to CRC off
    //status = setRegValue(SX127X_REG_MODEM_CONFIG_1, bw | cr | SX1278_HEADER_EXPL_MODE);
    status = setRegValue(SX127X_REG_MODEM_CONFIG_1, bw | cr | SX1278_HEADER_IMPL_MODE);
  }

  return (status);
}

uint8_t SX1276begin(uint8_t nss, uint8_t dio0, uint8_t dio1)
{
  // initialize low-level drivers
  //initModule(nss, dio0, dio1);
  DEBUG_PRINTLN("Init module SX1276");
  initModule(nss, dio1, dio0);

  // execute common part
  uint8_t status = SX127xDriver::SX127xBegin();
  if (status != ERR_NONE)
  {
    return (status);
  }

  // start configuration
  return (SX1276config(SX127xDriver::currBW, SX127xDriver::currSF, SX127xDriver::currCR, SX127xDriver::currFreq, SX127xDriver::_syncWord)); //check to see if this needs to be changed later
}
