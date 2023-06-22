#pragma once

#include "common.h"
#include "device.h"
#include "CRSF.h"

#if defined(USE_RADAR)

#define RADAR_ID_LEN        20
#define RADAR_SERIAL_LEN    17

enum RadarPhyMethod_e {
    rpmDisabled,
    rpmWifiBeacon,
    rpmWifiNan,
    rpmBluetoothLegacy,
    rpmBluetoothLongRange,
};

/* Stolen MSP definitions from the FormationFlight project */
struct msp_radar_pos_t {
  uint8_t id;
  uint8_t state;    // disarmed(0) armed (1)
  int32_t lat;      // decimal degrees latitude * 10000000
  int32_t lon;      // decimal degrees longitude * 10000000
  int32_t alt;      // cm
  uint16_t heading; // deg
  uint16_t speed;   // cm/s
  uint8_t lq;       // lq
} __attribute__((packed));

uint32_t Radar_GetSupportedPhyMethods();
void Radar_UpdatePosition(const crsf_sensor_gps_t *gps);

extern device_t Radar_device;

#endif
