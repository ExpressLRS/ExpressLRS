#ifndef RX_OTA_CONNECTOR_H
#define RX_OTA_CONNECTOR_H
#include "CRSFConnector.h"
#include "FIFO.h"

#if defined(PLATFORM_ESP32) && SOC_CPU_CORES_NUM > 1
#include <mutex>
#endif

#define TELEMETRY_FIFO_SIZE 512
typedef FIFO<TELEMETRY_FIFO_SIZE> TelemetryFifo;

enum CustomTelemSubTypeID : uint8_t {
    CRSF_AP_CUSTOM_TELEM_SINGLE_PACKET_PASSTHROUGH = 0xF0,
    CRSF_AP_CUSTOM_TELEM_STATUS_TEXT = 0xF1,
    CRSF_AP_CUSTOM_TELEM_MULTI_PACKET_PASSTHROUGH = 0xF2,
};

class RXOTAConnector : public CRSFConnector {
public:
    RXOTAConnector();
    void forwardMessage(const crsf_header_t *message) override;

    bool GetNextPayload(uint8_t* nextPayloadSize, uint8_t *payloadData);
    uint8_t GetFifoFullPct() const { return (TELEMETRY_FIFO_SIZE - messagePayloads.free()) * 100 / TELEMETRY_FIFO_SIZE; }

protected:
    TelemetryFifo messagePayloads;
    uint8_t prioritizedCount = 0;

private:
#if defined(PLATFORM_ESP32) && SOC_CPU_CORES_NUM > 1
    std::mutex mutex;
#endif
};

#endif //RX_OTA_CONNECTOR_H
