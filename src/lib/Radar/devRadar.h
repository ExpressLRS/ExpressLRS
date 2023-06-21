#pragma once

#include "common.h"
#include "device.h"
#include "CRSF.h"

#if defined(USE_RADAR)

enum RadarPhyMethod_e {
    rpmDisabled,
    rpmWifiBeacon,
    rpmWifiNan,
    rpmBluetoothLegacy,
    rpmBluetoothLongRange,
};

uint32_t Radar_GetSupportedPhyMethods();
void Radar_UpdatePosition(const crsf_sensor_gps_t *gps);

extern device_t Radar_device;

#endif
