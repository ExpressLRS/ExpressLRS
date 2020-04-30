#ifndef LoRa_SX1276
#define LoRa_SX1276

#include "LoRa_SX127x.h"

uint8_t SX1276config(SX127xDriver *drv, Bandwidth bw, SpreadingFactor sf, CodingRate cr, uint32_t freq, uint8_t syncWord);

class SX1276 : public SX127xDriver {
public:
    SX1276(int rst = -1, int dio0 = -1, int dio1 = -1, int txpin = -1, int rxpin = -1);
    uint8_t Config(Bandwidth bw, SpreadingFactor sf, CodingRate cr, uint32_t freq = 0, uint8_t syncWord = 0);

private:
};

#endif
