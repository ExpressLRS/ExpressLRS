#include "msp_utils.h"
#include <string.h>

void MSPUtils::init()
{
}

void MSPUtils::parseReset(msp_parse_status_t *status)
{
    status->state = MSP_STATE_IDLE;
    status->pos = 0;
    status->checksum = 0;
}

bool MSPUtils::parseToMsg(msp_message_t *msg, msp_parse_status_t *status, uint8_t c)
{
    switch (status->state) {
    case MSP_STATE_IDLE:
        if (c == MSP_MAGIC_1) {
            status->state = MSP_STATE_MAGIC1;
        }
        break;

    case MSP_STATE_MAGIC1:
        if (c == MSP_MAGIC_2_V2) {
            msg->magic2 = c;
            status->state = MSP_STATE_MAGIC2;
        } else if (c == MSP_MAGIC_2_V1) {
            msg->magic2 = c;
            status->state = MSP_STATE_MAGIC2;
        } else {
            parseReset(status);
        }
        break;

    case MSP_STATE_MAGIC2:
        msg->type = c;
        status->state = MSP_STATE_TYPE;
        break;

    case MSP_STATE_TYPE:
        if (msg->magic2 == MSP_MAGIC_2_V2) {
            msg->flag = c;
            status->state = MSP_STATE_FLAG;
            status->checksum = c;
        } else {
            msg->len = c;
            status->state = MSP_STATE_V1_LEN;
            status->checksum = c;
        }
        break;

    case MSP_STATE_FLAG:
        msg->function = c;
        status->state = MSP_STATE_FUNCTION_L;
        status->checksum = crc8Update(status->checksum, &c, 1);
        break;

    case MSP_STATE_FUNCTION_L:
        msg->function |= (uint16_t)c << 8;
        status->state = MSP_STATE_FUNCTION_H;
        status->checksum = crc8Update(status->checksum, &c, 1);
        break;

    case MSP_STATE_FUNCTION_H:
        msg->len = c;
        status->state = MSP_STATE_LEN_L;
        status->checksum = crc8Update(status->checksum, &c, 1);
        break;

    case MSP_STATE_LEN_L:
        msg->len |= (uint16_t)c << 8;
        status->state = (msg->len > 0) ? MSP_STATE_PAYLOAD : MSP_STATE_CHECKSUM;
        status->pos = 0;
        status->checksum = crc8Update(status->checksum, &c, 1);
        if (msg->len > MSP_PAYLOAD_LEN_MAX) {
            parseReset(status);
        }
        break;

    case MSP_STATE_PAYLOAD:
        msg->payload[status->pos++] = c;
        status->checksum = crc8Update(status->checksum, &c, 1);
        if (status->pos >= msg->len) {
            status->state = MSP_STATE_CHECKSUM;
        }
        break;

    case MSP_STATE_CHECKSUM:
        msg->checksum = c;
        parseReset(status);
        if (c == status->checksum) {
            msg->res = MSP_PARSE_RESULT_SUCCESS;
            return true;
        } else {
            msg->res = MSP_PARSE_RESULT_FAIL;
        }
        break;

    case MSP_STATE_V1_LEN:
        msg->function = c;
        msg->flag = 0;
        status->state = MSP_STATE_V1_FUNCTION;
        status->checksum ^= c;
        break;

    case MSP_STATE_V1_FUNCTION:
        status->state = (msg->len > 0) ? MSP_STATE_V1_PAYLOAD : MSP_STATE_V1_CHECKSUM;
        status->pos = 0;
        status->checksum ^= c;
        if (msg->len > MSP_PAYLOAD_LEN_MAX) {
            parseReset(status);
        }
        break;

    case MSP_STATE_V1_PAYLOAD:
        msg->payload[status->pos++] = c;
        status->checksum ^= c;
        if (status->pos >= msg->len) {
            status->state = MSP_STATE_V1_CHECKSUM;
        }
        break;

    case MSP_STATE_V1_CHECKSUM:
        msg->checksum = c;
        parseReset(status);
        if (c == status->checksum) {
            msg->res = MSP_PARSE_RESULT_SUCCESS;
            return true;
        } else {
            msg->res = MSP_PARSE_RESULT_FAIL;
        }
        break;
    }

    return false;
}

uint16_t MSPUtils::generateV2FrameBuf(uint8_t *buf, uint8_t type, uint8_t flag, uint16_t function, const uint8_t *payload, uint16_t len)
{
    uint16_t pos = 0;
    
    // 0: $
    buf[pos++] = MSP_MAGIC_1;
    // 1: X
    buf[pos++] = MSP_MAGIC_2_V2;
    // 2: Type (< or >)
    buf[pos++] = type;
    
    // --- CRC STARTS HERE (Index 3) ---
    // 3: Flag
    buf[pos++] = flag;
    // 4-5: Function ID (Little Endian)
    buf[pos++] = function & 0xFF;
    buf[pos++] = (function >> 8) & 0xFF;
    // 6-7: Payload Length (Little Endian)
    buf[pos++] = len & 0xFF;
    buf[pos++] = (len >> 8) & 0xFF;
    
    // 8 to (7 + len): Payload
    if (len > 0 && payload != nullptr) {
        memcpy(&buf[pos], payload, len);
        pos += len;
    }

    // The initial CRC value is 0.
    uint8_t crc = 0; 
    
    // Start at buf[3] (the Flag byte) and process 5 + len bytes.
    crc = crc8Update(crc, &buf[3], 5 + len); 
    
    // 8 + len: Checksum
    buf[pos++] = crc;
    
    return pos;
}

uint16_t MSPUtils::msgToFrameBuf(uint8_t *buf, const msp_message_t *msg)
{
    if (msg->magic2 == MSP_MAGIC_2_V2) {
        return generateV2FrameBuf(buf, msg->type, msg->flag, msg->function, msg->payload, msg->len);
    } else {
        uint16_t pos = 0;
        buf[pos++] = MSP_MAGIC_1;
        buf[pos++] = MSP_MAGIC_2_V1;
        buf[pos++] = msg->type;
        buf[pos++] = msg->len;
        buf[pos++] = msg->function;
        
        if (msg->len > 0) {
            memcpy(&buf[pos], msg->payload, msg->len);
            pos += msg->len;
        }
        
        uint8_t crc = msg->len ^ msg->function;
        for (uint16_t i = 0; i < msg->len; i++) {
            crc ^= msg->payload[i];
        }
        buf[pos++] = crc;
        
        return pos;
    }
}

uint8_t MSPUtils::crc8Update(uint8_t crc, const uint8_t *data, uint16_t len)
{
    for (uint16_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0xD5;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}