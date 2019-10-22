#include <Arduino.h>

#ifndef LoRa_SX1276
#define LoRa_SX1276

#include "LoRa_SX127x.h"

uint8_t SX1276begin(uint8_t nss, uint8_t dio0, uint8_t dio1);
uint8_t SX1276config(Bandwidth bw, SpreadingFactor sf, CodingRate cr, uint32_t freq, uint8_t syncWord);
uint8_t SX1276configCommon(uint8_t bw, uint8_t sf, uint8_t cr, uint32_t freq, uint8_t syncWord);

#endif