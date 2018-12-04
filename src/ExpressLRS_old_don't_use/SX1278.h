
//#include "TypeDef.h"
#include "RadioAPI_lowlevel.h"
//#include "SX127x.h"

//SX1278 specific register map
#define SX1278_REG_MODEM_CONFIG_3                     0x26
#define SX1278_REG_TCXO                               0x4B
#define SX1278_REG_PA_DAC                             0x4D
#define SX1278_REG_FORMER_TEMP                        0x5D
#define SX1278_REG_AGC_REF                            0x61
#define SX1278_REG_AGC_THRESH_1                       0x62
#define SX1278_REG_AGC_THRESH_2                       0x63
#define SX1278_REG_AGC_THRESH_3                       0x64
#define SX1278_REG_PLL                                0x70

//SX1278 LoRa modem settings
//SX1278_REG_OP_MODE                                                  MSB   LSB   DESCRIPTION
#define SX1278_HIGH_FREQ                              0b00000000  //  3     3     access HF test registers
#define SX1278_LOW_FREQ                               0b00001000  //  3     3     access LF test registers

//SX1278_REG_FRF_MSB + REG_FRF_MID + REG_FRF_LSB
#define SX1278_FRF_MSB                                0x6C        //  7     0     carrier frequency setting: f_RF = (F(XOSC) * FRF)/2^19
#define SX1278_FRF_MID                                0x80        //  7     0         where F(XOSC) = 32 MHz
#define SX1278_FRF_LSB                                0x00        //  7     0               FRF = 3 byte value of FRF registers

//SX1278_REG_PA_CONFIG
#define SX1278_MAX_POWER                              0b01110000  //  6     4     max power: P_max = 10.8 + 0.6*MAX_POWER [dBm]; P_max(MAX_POWER = 0b111) = 15 dBm
//#define SX1278_MAX_POWER                              0b00010000  //  6     4     changed

//SX1278_REG_LNA
#define SX1278_LNA_BOOST_LF_OFF                       0b00000000  //  4     3     default LNA current

//SX1278_REG_MODEM_CONFIG_1
#define SX1278_BW_7_80_KHZ                            0b00000000  //  7     4     bandwidth:  7.80 kHz
#define SX1278_BW_10_40_KHZ                           0b00010000  //  7     4                 10.40 kHz
#define SX1278_BW_15_60_KHZ                           0b00100000  //  7     4                 15.60 kHz
#define SX1278_BW_20_80_KHZ                           0b00110000  //  7     4                 20.80 kHz
#define SX1278_BW_31_25_KHZ                           0b01000000  //  7     4                 31.25 kHz
#define SX1278_BW_41_70_KHZ                           0b01010000  //  7     4                 41.70 kHz
#define SX1278_BW_62_50_KHZ                           0b01100000  //  7     4                 62.50 kHz
#define SX1278_BW_125_00_KHZ                          0b01110000  //  7     4                 125.00 kHz
#define SX1278_BW_250_00_KHZ                          0b10000000  //  7     4                 250.00 kHz
#define SX1278_BW_500_00_KHZ                          0b10010000  //  7     4                 500.00 kHz
#define SX1278_CR_4_5                                 0b00000010  //  3     1     error coding rate:  4/5
#define SX1278_CR_4_6                                 0b00000100  //  3     1                         4/6
#define SX1278_CR_4_7                                 0b00000110  //  3     1                         4/7
#define SX1278_CR_4_8                                 0b00001000  //  3     1                         4/8
#define SX1278_HEADER_EXPL_MODE                       0b00000000  //  0     0     explicit header mode
#define SX1278_HEADER_IMPL_MODE                       0b00000001  //  0     0     implicit header mode

//SX1278_REG_MODEM_CONFIG_2
#define SX1278_RX_CRC_MODE_OFF                        0b00000000  //  2     2     CRC disabled
#define SX1278_RX_CRC_MODE_ON                         0b00000100  //  2     2     CRC enabled

//SX1278_REG_MODEM_CONFIG_3
#define SX1278_LOW_DATA_RATE_OPT_OFF                  0b00000000  //  3     3     low data rate optimization disabled
#define SX1278_LOW_DATA_RATE_OPT_ON                   0b00001000  //  3     3     low data rate optimization enabled
#define SX1278_AGC_AUTO_OFF                           0b00000000  //  2     2     LNA gain set by REG_LNA
#define SX1278_AGC_AUTO_ON                            0b00000100  //  2     2     LNA gain set by internal AGC loop

//class SX1278: public SX127x {
//  public:
//SX1278(int nss, float freq, Bandwidth bw, SpreadingFactor sf, CodingRate cr, int dio0, int dio1, uint8_t syncWord);

//uint8_t configCommon(uint8_t bw, uint8_t sf, uint8_t cr, float freq, uint8_t syncWord);
//
//  private:
//int _nss;
//int _dio0;
//int _dio1;
//};
//
//#endif

uint8_t SX1278config(Bandwidth bw, SpreadingFactor sf, CodingRate cr, float freq, uint8_t syncWord);

