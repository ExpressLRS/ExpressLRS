#ifndef RadioAPI
#define RadioAPI


#include "SX127x.h"

#define ERR_NONE                        0x00
#define ERR_CHIP_NOT_FOUND              0x01
#define ERR_EEPROM_NOT_INITIALIZED      0x02

#define ERR_PACKET_TOO_LONG             0x10
#define ERR_TX_TIMEOUT                  0x11

#define ERR_RX_TIMEOUT                  0x20
#define ERR_CRC_MISMATCH                0x21

#define ERR_INVALID_BANDWIDTH           0x30
#define ERR_INVALID_SPREADING_FACTOR    0x31
#define ERR_INVALID_CODING_RATE         0x32
#define ERR_INVALID_FREQUENCY           0x33

#define ERR_INVALID_BIT_RANGE           0x40

#define CHANNEL_FREE                    0x50
#define PREAMBLE_DETECTED               0x51


#endif

//payload is 8 bytes
//syncword is 2 syms

//define the air rates to be dynamically switched between here

//AIRrate0 = {BW_500_00_KHZ, SF_6, CR_4_5, 915000000, SX127X_SYNC_WORD, 3.74}// 200hz -112
//AIRrate1 = (BW_250_00_KHZ, SF_6, CR_4_5, 915000000, SX127X_SYNC_WORD, 7.49) 100hz -115
//AIRrate2 = (BW_500_00_KHZ, SF_7, CR_4_5, 915000000, SX127X_SYNC_WORD, 7.49) 100hz -117
//AIRrate3 = (BW_125_00_KHZ, SF_6, CR_4_5, 915000000, SX127X_SYNC_WORD, 14.98) 50hz -118
//AIRrate4 = (BW_500_00_KHZ, SF_8, CR_4_5, 915000000, SX127X_SYNC_WORD, 12.42) 50hz -120
//AIRrate5 = (BW_250_00_KHZ, SF_7, CR_4_5, 915000000, SX127X_SYNC_WORD, 14.98) 50hz -120
//AIRrate6 = (BW_250_00_KHZ, SF_8, CR_4_5, 915000000, SX127X_SYNC_WORD, 24.83) 30hz -123
//AIRrate7 = (BW_125_00_KHZ, SF_7, CR_4_5, 915000000, SX127X_SYNC_WORD, 29.95) 25hz -123
//AIRrate8 = (BW_125_00_KHZ, SF_8, CR_4_5, 915000000, SX127X_SYNC_WORD, 49.66) 15hz -126
//AIRrate9 = (BW_125_00_KHZ, SF_9, CR_4_5, 915000000, SX127X_SYNC_WORD, 99.33) 10hz -129

////////////////////////////////////////////////////////////////////////////////////////////////

//enum AIRrate {Bandwidth, SpreadingFactor, CodingRate};




