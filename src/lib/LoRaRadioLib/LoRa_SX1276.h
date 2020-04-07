#ifndef LoRa_SX1276
#define LoRa_SX1276

#include "LoRa_SX127x.h"

class SX127xDriver;

uint8_t SX1276config(SX127xDriver *drv, Bandwidth bw, SpreadingFactor sf, CodingRate cr, uint32_t freq, uint8_t syncWord);

#endif
