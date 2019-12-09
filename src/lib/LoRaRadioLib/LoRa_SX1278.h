
#ifndef LoRa_SX1278
#define LoRa_SX1278

#include "LoRa_lowlevel.h"

uint8_t SX1278begin(uint8_t nss, uint8_t dio0, uint8_t dio1);
uint8_t SX1278config(Bandwidth bw, SpreadingFactor sf, CodingRate cr, uint32_t freq, uint8_t syncWord);
uint8_t SX1278configCommon(uint8_t bw, uint8_t sf, uint8_t cr, uint32_t freq, uint8_t syncWord);

#endif