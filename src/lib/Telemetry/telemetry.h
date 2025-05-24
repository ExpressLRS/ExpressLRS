#pragma once

#include <cstdint>
#include "crsf_protocol.h"
#include "CRSF.h"
#include "FIFO.h"

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
    void SetCrsfBatterySensorDetected();
    bool GetCrsfBatterySensorDetected() { return crsfBatterySensorDetected; };
    void CheckCrsfBaroSensorDetected();
    void SetCrsfBaroSensorDetected();
    bool GetCrsfBaroSensorDetected() { return crsfBaroSensorDetected; };
    uint8_t GetUpdatedModelMatch() { return modelMatchId; }
    bool GetNextPayload(uint8_t* nextPayloadSize, uint8_t **payloadData);
    int UpdatedPayloadCount();
    bool AppendTelemetryPackage(uint8_t *package);
private:
    FIFO<2048> messagePayloads;

    uint8_t currentPayload[CRSF_MAX_PACKET_LEN] {};

    bool processInternalTelemetryPackage(uint8_t *package);
    void AppendToPackage(volatile crsf_telemetry_package_t *current);
    uint8_t CRSFinBuffer[CRSF_MAX_PACKET_LEN];
    telemetry_state_s telemetry_state;
    uint8_t currentTelemetryByte;
    uint8_t currentPayloadIndex;
    uint8_t twoslotLastQueueIndex;
    volatile crsf_telemetry_package_t *telemetryPackageHead;
    uint8_t receivedPackages;
    bool callBootloader;
    bool callEnterBind;
    bool callUpdateModelMatch;
    bool sendDeviceFrame;
    bool crsfBatterySensorDetected;
    bool crsfBaroSensorDetected;
    uint8_t modelMatchId;
};
