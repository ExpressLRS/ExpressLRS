//#include "TypeDef.h"
//#include "SX1278.h"

//class SX1276: public SX1278 {
//  public:
//    SX1276(int nss, float freq, Bandwidth bw, SpreadingFactor sf, CodingRate cr, int dio0, int dio1, uint8_t syncWord);
//    
//    uint8_t config(Bandwidth bw, SpreadingFactor sf, CodingRate cr, float freq, uint8_t syncWord);
//};
//


//The below values were cut and paste from SX1278 values, they should be the same but just take note.
//SX1276_REG_MODEM_CONFIG_1
#define SX1276_BW_7_80_KHZ                            0b00000000  //  7     4     bandwidth:  7.80 kHz
#define SX1276_BW_10_40_KHZ                           0b00010000  //  7     4                 10.40 kHz
#define SX1276_BW_15_60_KHZ                           0b00100000  //  7     4                 15.60 kHz
#define SX1276_BW_20_80_KHZ                           0b00110000  //  7     4                 20.80 kHz
#define SX1276_BW_31_25_KHZ                           0b01000000  //  7     4                 31.25 kHz
#define SX1276_BW_41_70_KHZ                           0b01010000  //  7     4                 41.70 kHz
#define SX1276_BW_62_50_KHZ                           0b01100000  //  7     4                 62.50 kHz
#define SX1276_BW_125_00_KHZ                          0b01110000  //  7     4                 125.00 kHz
#define SX1276_BW_250_00_KHZ                          0b10000000  //  7     4                 250.00 kHz
#define SX1276_BW_500_00_KHZ                          0b10010000  //  7     4                 500.00 kHz
#define SX1276_CR_4_5                                 0b00000010  //  3     1     error coding rate:  4/5
#define SX1276_CR_4_6                                 0b00000100  //  3     1                         4/6
#define SX1276_CR_4_7                                 0b00000110  //  3     1                         4/7
#define SX1276_CR_4_8                                 0b00001000  //  3     1                         4/8
#define SX1276_HEADER_EXPL_MODE                       0b00000000  //  0     0     explicit header mode
#define SX1276_HEADER_IMPL_MODE                       0b00000001  //  0     0     implicit header mode


uint8_t SX1276config(Bandwidth bw, SpreadingFactor sf, CodingRate cr, float freq, uint8_t syncWord);