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
#include "FIFO_GENERIC.h"

void setupTargetCommon();
void deferExecution(uint32_t ms, std::function<void()> f);
void executeDeferredFunction(unsigned long now);
