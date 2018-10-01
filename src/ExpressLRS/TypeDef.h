#ifndef _LORALIB_TYPES_H
#define _LORALIB_TYPES_H

#if ARDUINO >= 100
  #include "Arduino.h"
#else
  #include "WProgram.h"
#endif

//#define DEBUG

#ifdef DEBUG
  //#define VERBOSE
  #define BYTE_TO_BINARY(byte)  \
    ((byte & 0x80) ? '1' : '0'), \
    ((byte & 0x40) ? '1' : '0'), \
    ((byte & 0x20) ? '1' : '0'), \
    ((byte & 0x10) ? '1' : '0'), \
    ((byte & 0x08) ? '1' : '0'), \
    ((byte & 0x04) ? '1' : '0'), \
    ((byte & 0x02) ? '1' : '0'), \
    ((byte & 0x01) ? '1' : '0') 
#endif

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

enum Chip {CH_SX1272, CH_SX1273, CH_SX1276, CH_SX1277, CH_SX1278, CH_SX1279};
enum Bandwidth {BW_7_80_KHZ, BW_10_40_KHZ, BW_15_60_KHZ, BW_20_80_KHZ, BW_31_25_KHZ, BW_41_70_KHZ, BW_62_50_KHZ, BW_125_00_KHZ, BW_250_00_KHZ, BW_500_00_KHZ};
enum SpreadingFactor {SF_6, SF_7, SF_8, SF_9, SF_10, SF_11, SF_12};
enum CodingRate {CR_4_5, CR_4_6, CR_4_7, CR_4_8};

#endif





