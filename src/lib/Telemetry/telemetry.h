#pragma once

#include "CRSFConnector.h"
#include "crsf_protocol.h"
#include "FIFO.h"

#if defined(PLATFORM_ESP32)
#include <mutex>
#endif

#define TELEMETRY_FIFO_SIZE 512
typedef FIFO<TELEMETRY_FIFO_SIZE> TelemetryFifo;

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

class Telemetry
{
public:
    Telemetry();
    bool RXhandleUARTin(CRSFConnector *origin, uint8_t data);
    void ResetState();
    void CheckCrsfBatterySensorDetected();
    void SetCrsfBatterySensorDetected();
    bool GetCrsfBatterySensorDetected() const { return crsfBatterySensorDetected; }
    void CheckCrsfBaroSensorDetected();
    void SetCrsfBaroSensorDetected();
    bool GetCrsfBaroSensorDetected() const { return crsfBaroSensorDetected; }
    bool GetNextPayload(uint8_t* nextPayloadSize, uint8_t *payloadData);
    int UpdatedPayloadCount();
    void AppendTelemetryPackage(uint8_t *package);
    uint8_t GetFifoFullPct() { return (TELEMETRY_FIFO_SIZE - messagePayloads.free()) * 100 / TELEMETRY_FIFO_SIZE; }
private:
#if defined(PLATFORM_ESP32) && SOC_CPU_CORES_NUM > 1
    std::mutex mutex;
#endif
    TelemetryFifo messagePayloads;

    uint8_t CRSFinBuffer[CRSF_MAX_PACKET_LEN];
    telemetry_state_s telemetry_state;
    uint8_t currentTelemetryByte;
    uint8_t prioritizedCount;
    bool sendDeviceFrame;
    bool crsfBatterySensorDetected;
    bool crsfBaroSensorDetected;
};
