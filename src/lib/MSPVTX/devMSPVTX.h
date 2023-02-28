#pragma once

#include "device.h"

extern device_t MSPVTx_device;

void mspVtxProcessPacket(uint8_t *packet);
void disableMspVtx(void);
