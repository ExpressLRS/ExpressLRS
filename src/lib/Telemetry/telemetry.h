#pragma once

#include <cstdint>
#include <crsf_protocol.h>

typedef enum {
    TELEMTRY_IDLE = 0,
    RECEIVING_LEGNTH,
    RECEIVING_DATA
} telemetry_state_s;

typedef struct crsf_telemetry_package_t {
    const uint8_t type;
    const uint8_t size;
    volatile bool locked;
    volatile bool updated;
    uint8_t *data;
} crsf_telemetry_package_t;

#define PAYLOAD_DATA(type0, type1, type2, type3, type4)\
    uint8_t PayloadData[\
        CRSF_TELEMETRY_TOTAL_SIZE(CRSF_FRAME_##type0##_PAYLOAD_SIZE) + \
        CRSF_TELEMETRY_TOTAL_SIZE(CRSF_FRAME_##type1##_PAYLOAD_SIZE) + \
        CRSF_TELEMETRY_TOTAL_SIZE(CRSF_FRAME_##type2##_PAYLOAD_SIZE) + \
        CRSF_TELEMETRY_TOTAL_SIZE(CRSF_FRAME_##type3##_PAYLOAD_SIZE) + \
        CRSF_TELEMETRY_TOTAL_SIZE(CRSF_FRAME_##type4##_PAYLOAD_SIZE)]; \
    crsf_telemetry_package_t payloadTypes[] = {\
    {CRSF_FRAMETYPE_##type0, CRSF_TELEMETRY_TOTAL_SIZE(CRSF_FRAME_##type0##_PAYLOAD_SIZE), false, false, 0},\
    {CRSF_FRAMETYPE_##type1, CRSF_TELEMETRY_TOTAL_SIZE(CRSF_FRAME_##type1##_PAYLOAD_SIZE), false, false, 0},\
    {CRSF_FRAMETYPE_##type2, CRSF_TELEMETRY_TOTAL_SIZE(CRSF_FRAME_##type2##_PAYLOAD_SIZE), false, false, 0},\
    {CRSF_FRAMETYPE_##type3, CRSF_TELEMETRY_TOTAL_SIZE(CRSF_FRAME_##type3##_PAYLOAD_SIZE), false, false, 0},\
    {CRSF_FRAMETYPE_##type4, CRSF_TELEMETRY_TOTAL_SIZE(CRSF_FRAME_##type4##_PAYLOAD_SIZE), false, false, 0}};\
    const uint8_t payloadTypesCount = (sizeof(payloadTypes)/sizeof(crsf_telemetry_package_t))

class Telemetry
{
public:
    bool RXhandleUARTin(uint8_t data);
    void ResetState();
    uint8_t* GetNextPayload();
    uint8_t* GetCurrentPayload();
    uint8_t UpdatedPayloadCount();
    uint8_t ReceivedPackagesCount();
    static bool callBootloader;

private:
    void AppendToPackage(volatile crsf_telemetry_package_t *current);
    void AppendTelemetryPackage();
    static uint8_t CRSFinBuffer[CRSF_MAX_PACKET_LEN];
    static telemetry_state_s telemtry_state;
    static uint8_t currentTelemetryByte;
    static uint8_t currentPayloadIndex;
    static volatile crsf_telemetry_package_t *telemetryPackageHead;
    static uint8_t receivedPackages;
};
