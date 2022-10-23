#pragma once

#include "targets.h"
#include "common.h"
#include "config.h"
#include "CRSF.h"
#include "FHSS.h"
#include "helpers.h"
#include "hwTimer.h"
#include "logging.h"
#include "LBT.h"
#include "LQCALC.h"
#include "OTA.h"
#include "POWERMGNT.h"

#if defined(USE_AIRPORT_AT_BAUD)
#include "FIFO_GENERIC.h"
#endif

void setupTargetCommon();
void deferExecution(uint32_t ms, std::function<void()> f);
void executeDeferredFunction(unsigned long now);

#if defined(USE_AIRPORT_AT_BAUD)
// Variables / constants for Airport //
FIFO_GENERIC<AP_MAX_BUF_LEN> apInputBuffer;
FIFO_GENERIC<AP_MAX_BUF_LEN> apOutputBuffer;
#endif
