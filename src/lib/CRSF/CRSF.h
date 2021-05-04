#pragma once

#include "targets.h"

#if CRSF_RX_MODULE        
#include "crsf_rxmodule.h"
#endif

#if CRSF_TX_MODULE
#include "crsf_txmodule.h"
#endif

// Serial settings:
//  TODO: this needs to go away
extern uint32_t UARTwdtLastChecked;
extern uint32_t UARTcurrentBaud;

// for the UART wdt, every 1000ms we change bauds when connect is lost
#define UARTwdtInterval 1000
