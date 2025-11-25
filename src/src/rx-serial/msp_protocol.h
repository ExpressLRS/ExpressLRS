#pragma once

#include <stdint.h>

#ifndef PACKED
#define MSP_PACKED(__Declaration__)  __Declaration__ __attribute__((packed))
#else
#define MSP_PACKED  PACKED
#endif

#define MSP_PAYLOAD_LEN_MAX       512

#define MSP_MAGIC_1               '$'
#define MSP_MAGIC_2_V1            'M'
#define MSP_MAGIC_2_V2            'X'
#define MSP_V1_HEADER_LEN         5
#define MSP_V2_HEADER_LEN         8

#define MSP_FRAME_LEN_MAX         (8 + MSP_PAYLOAD_LEN_MAX + 1)

typedef enum {
    MSP_TYPE_REQUEST    = '<',
    MSP_TYPE_RESPONSE   = '>',
    MSP_TYPE_ERROR      = '!',
} MSP_TYPE_ENUM;

typedef enum {
    MSP_FLAG_NONE               = 0,
    MSP_FLAG_NO_RESPONSE        = 0x01,
    MSP_FLAG_SOURCE_ID_RC_LINK  = 0x02,
} MSP_FLAG_ENUM;

typedef enum {
    MSP_FC_STATUS                 = 101,
    MSP_RAW_IMU                   = 102,
    MSP_RAW_GPS                   = 106,
    MSP_COMP_GPS                  = 107,
    MSP_ATTITUDE                  = 108,
    MSP_ALTITUDE                  = 109,
    MSP_ANALOG                    = 110,
    MSP_ACTIVEBOXES               = 113,
    MSP_SENSOR_STATUS             = 151,
    MSP_SET_RAW_RC                = 200,

    MSP2_INAV_STATUS              = 0x2000,
    MSP2_INAV_ANALOG              = 0x2002,
    MSP2_INAV_MISC2               = 0x203A,
    MSP2_COMMON_SET_MSP_RC_LINK_STATS   = 0x100D,
    MSP2_COMMON_SET_MSP_RC_INFO         = 0x100E,
} MSP_FUNCTION_ENUM;

typedef struct {
    uint8_t magic2;
    uint8_t type;
    uint8_t flag;
    uint16_t function;
    uint16_t len;
    uint8_t payload[MSP_PAYLOAD_LEN_MAX];
    uint8_t checksum;
    uint8_t res;
} msp_message_t;

typedef struct
{
    uint16_t rc[16];
} __attribute__((packed)) tMspSetRawRc;

#define MSP_SET_RAW_RC_LEN  32

typedef struct
{
    uint8_t sublink_id;
    uint8_t valid_link;
    uint8_t uplink_rssi_perc;
    uint8_t uplink_rssi;
    uint8_t downlink_link_quality;
    uint8_t uplink_link_quality;
    uint8_t uplink_snr;
} __attribute__((packed)) tMspCommonSetMspRcLinkStats;

#define MSP_COMMON_SET_MSP_RC_LINK_STATS_LEN  7

typedef struct
{
    uint8_t sublink_id;
    uint16_t uplink_tx_power;
    uint16_t downlink_tx_power;
    char band[4];
    char mode[6];
} __attribute__((packed)) tMspCommonSetMspRcInfo;

#define MSP_COMMON_SET_MSP_RC_INFO_LEN  15

typedef enum {
    MSP_PARSE_RESULT_FAIL = 0,
    MSP_PARSE_RESULT_SUCCESS,
    MSP_PARSE_RESULT_PROGRESS,
} MSP_PARSE_RESULT_ENUM;

typedef struct {
    uint8_t state;
    uint8_t pos;
    uint8_t checksum;
} msp_parse_status_t;