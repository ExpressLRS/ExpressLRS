#pragma once

#include <cstdint>
#include <crsf_protocol.h>

typedef enum {
    TELEMTRY_IDLE = 0,
    RECEIVING_LEGNTH,
    RECEIVING_DATA
} telemetry_state_s;

typedef struct crsf_telemetry_package_t {
    uint8_t *data;
    volatile bool locked;
    volatile struct crsf_telemetry_package_t* next;
} crsf_telemetry_package_t;

enum {
    CRSF_FRAME_GPS_PAYLOAD_SIZE = 15,
    CRSF_FRAME_BATTERY_SENSOR_PAYLOAD_SIZE = 8,
    CRSF_FRAME_ATTITUDE_PAYLOAD_SIZE = 6,
};

class Telemetry
{
public:
    bool RXhandleUARTin(uint8_t data);
    void ResetState();
    static bool callBootloader;

private:
    void AppendToPackage(volatile crsf_telemetry_package_t *current);
    void AppendTelemetryPackage();
    static telemetry_state_s telemtry_state;
    static uint8_t CRSFinBuffer[CRSF_MAX_PACKET_LEN];
    static uint8_t currentTelemetryByte;
    static volatile crsf_telemetry_package_t *telemetryPackageHead;
};