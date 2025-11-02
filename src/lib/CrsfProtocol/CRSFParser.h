#ifndef CRSF_PARSER_H
#define CRSF_PARSER_H

#include <targets.h>
#include <functional>

#include "crsf_protocol.h"
#include "CRSFConnector.h"

class CRSFParser {
public:
    void processBytes(CRSFConnector *origin, const uint8_t *inputBytes, uint16_t size, const std::function<void(const crsf_header_t *)> &foundMessage = nullptr);
    bool processByte(CRSFConnector *origin, uint8_t inputByte, const std::function<void(const crsf_header_t *)>& foundMessage = nullptr);

    // unit testing
    void Reset()
    {
        telemetry_state = TELEMETRY_IDLE;
        inBufferIndex = 0;
    }

private:
    // For processing incoming bytes
    typedef enum {
        TELEMETRY_IDLE = 0,
        RECEIVING_LENGTH,
        RECEIVING_DATA
    } telemetry_state_s;

    telemetry_state_s telemetry_state = TELEMETRY_IDLE;
    uint8_t inBufferIndex = 0;
    uint8_t CRSFinBuffer[CRSF_MAX_PACKET_LEN] = {};

};

#endif //CRSF_PARSER_H
