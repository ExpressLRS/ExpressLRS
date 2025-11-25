#pragma once

#include "msp_protocol.h"

class MSPUtils {
public:
    static void init();
    static void parseReset(msp_parse_status_t *status);
    static bool parseToMsg(msp_message_t *msg, msp_parse_status_t *status, uint8_t c);
    static uint16_t generateV2FrameBuf(uint8_t *buf, uint8_t type, uint8_t flag, uint16_t function, const uint8_t *payload, uint16_t len);
    static uint16_t msgToFrameBuf(uint8_t *buf, const msp_message_t *msg);
    static uint8_t crc8Update(uint8_t crc, const uint8_t *data, uint16_t len);

private:
    enum {
        MSP_STATE_IDLE = 0,
        MSP_STATE_MAGIC1,
        MSP_STATE_MAGIC2,
        MSP_STATE_TYPE,
        MSP_STATE_FLAG,
        MSP_STATE_FUNCTION_L,
        MSP_STATE_FUNCTION_H,
        MSP_STATE_LEN_L,
        MSP_STATE_LEN_H,
        MSP_STATE_PAYLOAD,
        MSP_STATE_CHECKSUM,
        MSP_STATE_V1_LEN,
        MSP_STATE_V1_FUNCTION,
        MSP_STATE_V1_PAYLOAD,
        MSP_STATE_V1_CHECKSUM,
    };
};