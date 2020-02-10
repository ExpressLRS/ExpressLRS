#include "debug.h"

// void ICACHE_RAM_ATTR PrintRC()
// {
//   DEBUG_PRINT(crsf.ChannelDataIn[0]);
//   DEBUG_PRINT(",");
//   DEBUG_PRINT(crsf.ChannelDataIn[1]);
//   DEBUG_PRINT(",");
//   DEBUG_PRINT(crsf.ChannelDataIn[2]);
//   DEBUG_PRINT(",");
//   DEBUG_PRINT(crsf.ChannelDataIn[3]);
//   DEBUG_PRINT(",");
//   DEBUG_PRINTLN(crsf.ChannelDataIn[4]);
//   DEBUG_PRINT(",");
//   DEBUG_PRINT(crsf.ChannelDataIn[5]);
//   DEBUG_PRINT(",");
//   DEBUG_PRINTLN(crsf.ChannelDataIn[6]);
//   DEBUG_PRINT(",");
//   DEBUG_PRINTLN(crsf.ChannelDataIn[7]);
// }

extern unsigned long seed;

long rng(void);

void rngSeed(long newSeed);
// 0..255 returned
long rng8Bit(void);
// 0..31 returned
long rng5Bit(void);