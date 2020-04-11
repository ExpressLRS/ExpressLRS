#pragma once

#define MSP_ELRS_FUNC       0x4578 // ['E','x']

// ELRS specific opcodes
#define MSP_ELRS_RF_MODE    0x00
#define MSP_ELRS_TX_PWR     0x01
#define MSP_ELRS_TLM_RATE   0x02

#define CHECK_PACKET_PARSING() \
  if (packet->readError) {\
    return;\
  }
