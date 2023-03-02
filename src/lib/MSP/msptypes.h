#pragma once

#define MSP_ELRS_FUNC       0x4578 // ['E','x']

#define MSP_SET_RX_CONFIG   45
#define MSP_VTX_CONFIG      88   //out message         Get vtx settings - betaflight
#define MSP_SET_VTX_CONFIG  89   //in message          Set vtx settings - betaflight
#define MSP_EEPROM_WRITE    250  //in message          no param

// ELRS specific opcodes
#define MSP_ELRS_RF_MODE                    0x06    // NOTIMPL
#define MSP_ELRS_TX_PWR                     0x07    // NOTIMPL
#define MSP_ELRS_TLM_RATE                   0x08    // NOTIMPL
#define MSP_ELRS_BIND                       0x09
#define MSP_ELRS_MODEL_ID                   0x0A
#define MSP_ELRS_REQU_VTX_PKT               0x0B
#define MSP_ELRS_SET_TX_BACKPACK_WIFI_MODE  0x0C
#define MSP_ELRS_SET_VRX_BACKPACK_WIFI_MODE 0x0D
#define MSP_ELRS_SET_RX_WIFI_MODE           0x0E
#define MSP_ELRS_SET_RX_LOAN_MODE           0x0F
#define MSP_ELRS_GET_BACKPACK_VERSION       0x10

#define MSP_ELRS_POWER_CALI_GET             0x20
#define MSP_ELRS_POWER_CALI_SET             0x21

// CRSF encapsulated msp defines
#define ENCAPSULATED_MSP_HEADER_CRC_LEN     4
#define ENCAPSULATED_MSP_MAX_PAYLOAD_SIZE   4
#define ENCAPSULATED_MSP_MAX_FRAME_LEN      (ENCAPSULATED_MSP_HEADER_CRC_LEN + ENCAPSULATED_MSP_MAX_PAYLOAD_SIZE)

// ELRS backpack protocol opcodes
// See: https://docs.google.com/document/d/1u3c7OTiO4sFL2snI-hIo-uRSLfgBK4h16UrbA08Pd6U/edit#heading=h.1xw7en7jmvsj
#define MSP_ELRS_BACKPACK_GET_CHANNEL_INDEX     0x0300
#define MSP_ELRS_BACKPACK_SET_CHANNEL_INDEX     0x0301
#define MSP_ELRS_BACKPACK_GET_FREQUENCY         0x0302
#define MSP_ELRS_BACKPACK_SET_FREQUENCY         0x0303
#define MSP_ELRS_BACKPACK_GET_RECORDING_STATE   0x0304
#define MSP_ELRS_BACKPACK_SET_RECORDING_STATE   0x0305
#define MSP_ELRS_BACKPACK_GET_VRX_MODE          0x0306
#define MSP_ELRS_BACKPACK_SET_VRX_MODE          0x0307
#define MSP_ELRS_BACKPACK_GET_RSSI              0x0308
#define MSP_ELRS_BACKPACK_GET_BATTERY_VOLTAGE   0x0309
#define MSP_ELRS_BACKPACK_GET_FIRMWARE          0x030A
#define MSP_ELRS_BACKPACK_SET_BUZZER            0x030B
#define MSP_ELRS_BACKPACK_SET_OSD_ELEMENT       0x030C
