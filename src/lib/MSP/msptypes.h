#pragma once

#define MSP_ELRS_FUNC       0x4578 // ['E','x']

#define MSP_SET_RX_CONFIG   45
#define MSP_VTX_CONFIG      88   //out message         Get vtx settings - betaflight
#define MSP_SET_VTX_CONFIG  89   //in message          Set vtx settings - betaflight
#define MSP_EEPROM_WRITE    250  //in message          no param

// ELRS specific opcodes
#define MSP_ELRS_RF_MODE    0x06
#define MSP_ELRS_TX_PWR     0x07
#define MSP_ELRS_TLM_RATE   0x08
#define MSP_ELRS_BIND       0x09
#define MSP_ELRS_MODEL_ID   0x0A

// CRSF encapsulated msp defines
#define ENCAPSULATED_MSP_PAYLOAD_SIZE 4
#define ENCAPSULATED_MSP_FRAME_LEN    8
