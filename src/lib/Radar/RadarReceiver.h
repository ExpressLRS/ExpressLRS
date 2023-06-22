#pragma once

#include "devRadar.h"

#if defined(USE_RADAR)

#define MAC_LEN 6

typedef struct tagRadarPilotInfo {
    uint8_t mac[MAC_LEN]; // because we can't rely on users to not fly two things with the same OID
    char operator_id[RADAR_ID_LEN+1];
    int8_t rssi;         // dBm
    uint32_t last_seen;  // ms
    int32_t latitude;    // in * 10M
    int32_t longitude;   // in * 10M
    int16_t altitude;    // in m
    uint16_t heading;
    uint16_t speed;
    uint8_t dirty;       // set to 1 on update
} RadarPilotInfo;

void RadarRx_Begin();
void RadarRx_End();

#define RADAR_PILOT_IDX_INVALID 0xff

uint8_t Radar_FindNextDirtyIdx();
extern RadarPilotInfo pilots[];

#endif