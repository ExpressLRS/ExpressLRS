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

class Telemetry
{
public:
    bool RXhandleUARTin(uint8_t data);
    void ResetState();
    volatile crsf_telemetry_package_t* GetPackageHead();
    void DeleteHead();
    uint8_t QueueLength();
    static bool callBootloader;

private:
    void AppendToPackage(volatile crsf_telemetry_package_t *current);
    void AppendTelemetryPackage();
    static telemetry_state_s telemtry_state;
    static uint8_t CRSFinBuffer[CRSF_MAX_PACKET_LEN];
    static uint8_t currentTelemetryByte;
    static volatile crsf_telemetry_package_t *telemetryPackageHead;
};