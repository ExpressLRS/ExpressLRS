#pragma once

#define MSP_ELRS_FUNC       0x4578 // ['E','x']

// ELRS specific opcodes
#define MSP_ELRS_RF_MODE    0x00
#define MSP_ELRS_TX_PWR     0x01
#define MSP_ELRS_TLM_RATE   0x02

// CRSF encapsulated msp defines
#define ENCAPSULATED_MSP_PAYLOAD_SIZE   6

#define TELEMETRY_MSP_VERSION    1
#define TELEMETRY_MSP_VER_SHIFT  5
#define TELEMETRY_MSP_VER_MASK   (0x7 << TELEMETRY_MSP_VER_SHIFT)
#define TELEMETRY_MSP_SEQ_MASK   0x0F

#define CHECK_PACKET_PARSING() \
  if (packet->readError) {\
    return;\
  }
