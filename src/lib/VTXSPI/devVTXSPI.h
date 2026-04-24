#pragma once

#include "device.h"

extern device_t VTxSPI_device;

extern uint16_t vtxSPIFrequency;
extern uint8_t vtxSPIPowerIdx;
extern uint8_t vtxSPIPitmode;

void VTxOutputMinimum();
void disableVTxSpi();
