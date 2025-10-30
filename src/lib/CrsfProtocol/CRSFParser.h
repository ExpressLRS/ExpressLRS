#ifndef CRSF_PARSER_H
#define CRSF_PARSER_H

#include <targets.h>
#include <functional>

#include "crsf_protocol.h"
#include "CRSFConnector.h"

class CRSFParser {
public:
    bool processByte(CRSFConnector *origin, uint8_t byte, const std::function<void(const crsf_header_t *)>& foundMessage = nullptr);

    // unit testing
    void Reset()
    {
        telemetry_state = TELEMETRY_IDLE;
        currentTelemetryByte = 0;
    }

private:
    // For processing incoming bytes
    typedef enum {
        TELEMETRY_IDLE = 0,
        RECEIVING_LENGTH,
        RECEIVING_DATA
    } telemetry_state_s;

    telemetry_state_s telemetry_state = TELEMETRY_IDLE;
    uint8_t currentTelemetryByte = 0;
    uint8_t CRSFinBuffer[CRSF_MAX_PACKET_LEN] = {};

};

#endif //CRSF_PARSER_H
