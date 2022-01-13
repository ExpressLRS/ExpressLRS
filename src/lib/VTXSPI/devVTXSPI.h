#pragma once

#include "device.h"

#if defined(GPIO_PIN_SPI_VTX_NSS) && (GPIO_PIN_SPI_VTX_NSS != UNDEF_PIN)

#define HAS_VTX_SPI

extern device_t VTxSPI_device;

extern uint8_t vtxSPIBandChannelIdx;
extern uint8_t vtxSPIBandChannelIdxCurrent;
extern uint8_t vtxSPIPowerIdx;
extern uint8_t vtxSPIPitmode;

#endif
