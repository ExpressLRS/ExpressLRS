
#ifndef LoRa_SX1278
#define LoRa_SX1278

#include "LoRa_lowlevel.h"

uint8_t SX1278config(SX127xDriver *drv, Bandwidth bw, SpreadingFactor sf, CodingRate cr, uint32_t freq, uint8_t syncWord);

#endif
