#pragma once

#include <cstdint>
#include "crsf_protocol.h"
#include "devCRSF.h"

enum CustomTelemSubTypeID : uint8_t {
    CRSF_AP_CUSTOM_TELEM_SINGLE_PACKET_PASSTHROUGH = 0xF0,
    CRSF_AP_CUSTOM_TELEM_STATUS_TEXT = 0xF1,
    CRSF_AP_CUSTOM_TELEM_MULTI_PACKET_PASSTHROUGH = 0xF2,
};

typedef enum {
    TELEMETRY_IDLE = 0,
    RECEIVING_LENGTH,
    RECEIVING_DATA
} telemetry_state_s;

typedef struct crsf_telemetry_package_t {
    const uint8_t type;
    const uint8_t size;
    volatile bool locked;
    volatile bool updated;
    uint8_t *data;
} crsf_telemetry_package_t;

#define PAYLOAD_DATA(type0, type1, type2, type3, type4, type5, type6)\
    uint8_t PayloadData[\
        CRSF_TELEMETRY_TOTAL_SIZE(CRSF_FRAME_##type0##_PAYLOAD_SIZE) + \
        CRSF_TELEMETRY_TOTAL_SIZE(CRSF_FRAME_##type1##_PAYLOAD_SIZE) + \
        CRSF_TELEMETRY_TOTAL_SIZE(CRSF_FRAME_##type2##_PAYLOAD_SIZE) + \
        CRSF_TELEMETRY_TOTAL_SIZE(CRSF_FRAME_##type3##_PAYLOAD_SIZE) + \
        CRSF_TELEMETRY_TOTAL_SIZE(CRSF_FRAME_##type4##_PAYLOAD_SIZE) + \
        CRSF_TELEMETRY_TOTAL_SIZE(CRSF_FRAME_##type5##_PAYLOAD_SIZE) + \
        CRSF_TELEMETRY_TOTAL_SIZE(CRSF_FRAME_##type6##_PAYLOAD_SIZE) + \
        CRSF_TELEMETRY_TOTAL_SIZE(CRSF_FRAME_GENERAL_RESP_PAYLOAD_SIZE) + \
        CRSF_TELEMETRY_TOTAL_SIZE(CRSF_FRAME_GENERAL_RESP_PAYLOAD_SIZE)]; \
    crsf_telemetry_package_t payloadTypes[] = {\
    {CRSF_FRAMETYPE_##type0, CRSF_TELEMETRY_TOTAL_SIZE(CRSF_FRAME_##type0##_PAYLOAD_SIZE), false, false, 0},\
    {CRSF_FRAMETYPE_##type1, CRSF_TELEMETRY_TOTAL_SIZE(CRSF_FRAME_##type1##_PAYLOAD_SIZE), false, false, 0},\
    {CRSF_FRAMETYPE_##type2, CRSF_TELEMETRY_TOTAL_SIZE(CRSF_FRAME_##type2##_PAYLOAD_SIZE), false, false, 0},\
    {CRSF_FRAMETYPE_##type3, CRSF_TELEMETRY_TOTAL_SIZE(CRSF_FRAME_##type3##_PAYLOAD_SIZE), false, false, 0},\
    {CRSF_FRAMETYPE_##type4, CRSF_TELEMETRY_TOTAL_SIZE(CRSF_FRAME_##type4##_PAYLOAD_SIZE), false, false, 0},\
    {CRSF_FRAMETYPE_##type5, CRSF_TELEMETRY_TOTAL_SIZE(CRSF_FRAME_##type5##_PAYLOAD_SIZE), false, false, 0},\
    {CRSF_FRAMETYPE_##type6, CRSF_TELEMETRY_TOTAL_SIZE(CRSF_FRAME_##type6##_PAYLOAD_SIZE), false, false, 0},\
    {0, CRSF_TELEMETRY_TOTAL_SIZE(CRSF_FRAME_GENERAL_RESP_PAYLOAD_SIZE), false, false, 0},\
    {0, CRSF_TELEMETRY_TOTAL_SIZE(CRSF_FRAME_GENERAL_RESP_PAYLOAD_SIZE), false, false, 0}};\
    const uint8_t payloadTypesCount = (sizeof(payloadTypes)/sizeof(crsf_telemetry_package_t))

class Telemetry
{
public:
    Telemetry();
    bool RXhandleUARTin(uint8_t data);
    void ResetState();
    bool ShouldCallBootloader();
    bool ShouldCallEnterBind();
    bool ShouldCallUpdateModelMatch();
    bool ShouldSendDeviceFrame();
    void CheckCrsfBatterySensorDetected();
    bool GetCrsfBatterySensorDetected() { return crsfBatterySensorDetected; };
    uint8_t GetUpdatedModelMatch() { return modelMatchId; }
    bool GetNextPayload(uint8_t* nextPayloadSize, uint8_t **payloadData);
    uint8_t UpdatedPayloadCount();
    uint8_t ReceivedPackagesCount();
    bool AppendTelemetryPackage(uint8_t *package);
private:
    void AppendToPackage(volatile crsf_telemetry_package_t *current);
    uint8_t CRSFinBuffer[CRSF_MAX_PACKET_LEN];
    telemetry_state_s telemetry_state;
    uint8_t currentTelemetryByte;
    uint8_t currentPayloadIndex;
    volatile crsf_telemetry_package_t *telemetryPackageHead;
    uint8_t receivedPackages;
    bool callBootloader;
    bool callEnterBind;
    bool callUpdateModelMatch;
    bool sendDeviceFrame;
    bool crsfBatterySensorDetected;
    uint8_t modelMatchId;
};
